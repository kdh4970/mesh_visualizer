// 240402
// Test Mesh Visualizer
#include <iostream>
#include <GL/freeglut.h>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#include "helper_gl.h"
#include "helper_cuda.h"

#define REFRESH_DELAY 10

GLuint posVbo;
GLint gl_Shader;
struct cudaGraphicsResource *cuda_posvbo_resource;


float4 *d_pos = 0;
uint totalVerts = 0;

unsigned int window_width = 2560;
unsigned int window_height = 1440;

// mouse controls
float _fovy = 30.0;
int mouse_old_x, mouse_old_y;
int mouse_buttons = 0;
float3 rotate = make_float3(-135.0, 90.0, 45.0);
float3 translate = make_float3(0.0, 0.5, -1.0);

// toggles
bool wireframe = false;
bool lighting = false;
bool render = true;

struct mesh_t
{
  int num_vertices;
  int num_triangles;
  float4* vertices;
  uint3* triangles;
};

// forward declarations
bool initGL(int *argc, char** argv);
void initMesh();
void renderIsosurface();
void cleanup();
GLuint compileASMShader(GLenum program_type, const char *code);
void createVBO(GLuint *vbo, unsigned int size);
void deleteVBO(GLuint *vbo, struct cudaGraphicsResource **cuda_resource);
void display();
void keyboard(unsigned char key, int /*x*/, int /*y*/);
void mouse(int button, int state, int x, int y);
void motion(int x, int y);
void reshape(int w, int h);
void timerEvent(int value);
void mainMenu(int i);
void initMenus();
mesh_t read_mesh(const std::string& filename);
void meshcopyHtoD(mesh_t mesh);

int main(int argc, char** argv)
{

  std::string filename = "/home/do/Desktop/do_code/mesh_deduplicator_cuda/data/sample_mesh_data.txt";
  mesh_t mesh = read_mesh(filename);
  
  if(false == initGL(&argc, argv))
  {
    std::cout << "Failed to initialize OpenGL" << std::endl;
  }
  initMesh();

  meshcopyHtoD(mesh);
  // glut Main Visuzliaztion Loop
  glutMainLoop();

  return 0;
}

// shader for displaying floating-point texture
static const char *shader_code =
    "!!ARBfp1.0\n"
    "TEX result.color, fragment.texcoord, texture[0], 2D; \n"
    "END";

GLuint compileASMShader(GLenum program_type, const char *code) {
  GLuint program_id;
  glGenProgramsARB(1, &program_id);
  glBindProgramARB(program_type, program_id);
  glProgramStringARB(program_type, GL_PROGRAM_FORMAT_ASCII_ARB,
                     (GLsizei)strlen(code), (GLubyte *)code);

  GLint error_pos;
  glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);

  if (error_pos != -1) {
    const GLubyte *error_string;
    error_string = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    fprintf(stderr, "Program error at position: %d\n%s\n", (int)error_pos,
            error_string);
    return 0;
  }

  return program_id;
}

bool initGL(int *argc, char** argv)
{
  // Initialize GLUT
  glutInit(argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
  glutInitWindowSize(window_width, window_height);
  glutCreateWindow("Mesh Visualizer");

  glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
  glEnable(GL_DEPTH_TEST);

  
  // good old-fashioned fixed function lighting
  float black[] = {0.0f, 0.0f, 0.0f, 1.0f};
  float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
  float ambient[] = {0.1f, 0.1f, 0.1f, 1.0f};
  float diffuse[] = {0.9f, 0.9f, 0.9f, 1.0f};
  float lightPos[] = {0.0f, 0.0f, 1.0f, 0.0f};

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);

  glLightfv(GL_LIGHT0, GL_AMBIENT, white);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
  glLightfv(GL_LIGHT0, GL_SPECULAR, white);
  glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, black);

  glEnable(GL_LIGHT0);
  // glEnable(GL_NORMALIZE);

  // load shader program
  // gl_Shader = compileASMShader(GL_FRAGMENT_PROGRAM_ARB, shader_code);


  // Set glut Functions
  glutDisplayFunc(display);
  glutKeyboardFunc(keyboard);
  glutReshapeFunc(reshape);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
  glutTimerFunc(REFRESH_DELAY, timerEvent, 0);


  initMenus();

  glutReportErrors();
  return true;
}

void initMesh()
{
  size_t maxVerts = 1e6;
  // create VBOs
  createVBO(&posVbo, maxVerts * sizeof(float) * 4);
  // DEPRECATED: checkCudaErrors( cudaGLRegisterBufferObject(posVbo) );
  checkCudaErrors(cudaGraphicsGLRegisterBuffer(
      &cuda_posvbo_resource, posVbo, cudaGraphicsMapFlagsWriteDiscard));

  
  // createVBO(&normalVbo, maxVerts * sizeof(float) * 4);
  // // DEPRECATED: checkCudaErrors(cudaGLRegisterBufferObject(normalVbo));
  // checkCudaErrors(cudaGraphicsGLRegisterBuffer(
  //     &cuda_normalvbo_resource, normalVbo, cudaGraphicsMapFlagsWriteDiscard));
  

}

////////////////////////////////////////////////////////////////////////////////
// Render isosurface geometry from the vertex buffers
////////////////////////////////////////////////////////////////////////////////
void renderIsosurface() {
  glBindBuffer(GL_ARRAY_BUFFER, posVbo);
  glVertexPointer(4, GL_FLOAT, 0, 0);
  glEnableClientState(GL_VERTEX_ARRAY);

  // glColor3f(0.6, 0.6, 0.6);
  // glDrawArrays(GL_TRIANGLES, 0, totalVerts);

  // 면 렌더링
  glColor3f(1.0, 1.0, 1.0);
  glDrawArrays(GL_TRIANGLES, 0, totalVerts);

  // 엣지 렌더링
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glColor3f(0.0, 0.1, 0.0);
  glDrawArrays(GL_TRIANGLES, 0, totalVerts);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // 다시 면 렌더링 모드로 변경

  glDisableClientState(GL_VERTEX_ARRAY);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void cleanup() {
  deleteVBO(&posVbo, &cuda_posvbo_resource);
  // deleteVBO(&normalVbo, &cuda_normalvbo_resource);
  
}



////////////////////////////////////////////////////////////////////////////////
//! Create VBO
////////////////////////////////////////////////////////////////////////////////
void createVBO(GLuint *vbo, unsigned int size) {
  // create buffer object
  glGenBuffers(1, vbo);
  glBindBuffer(GL_ARRAY_BUFFER, *vbo);

  // initialize buffer object
  glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glutReportErrors();
}

////////////////////////////////////////////////////////////////////////////////
//! Delete VBO
////////////////////////////////////////////////////////////////////////////////
void deleteVBO(GLuint *vbo, struct cudaGraphicsResource **cuda_resource) {
  glBindBuffer(1, *vbo);
  glDeleteBuffers(1, vbo);
  // DEPRECATED: checkCudaErrors(cudaGLUnregisterBufferObject(*vbo));
  cudaGraphicsUnregisterResource(*cuda_resource);

  *vbo = 0;
}

void display()
{
  {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(translate.x, translate.y, translate.z);
    glRotatef(rotate.x, 1.0, 0.0, 0.0);
    glRotatef(rotate.y, 0.0, 1.0, 0.0);

    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    

    if (lighting) {
      glEnable(GL_LIGHTING);
    }

    // render
    if (render) {
      glPushMatrix();
      glRotatef(180.0, 0.0, 1.0, 0.0);
      glRotatef(90.0, 1.0, 0.0, 0.0);
      renderIsosurface();
      glPopMatrix();
    }
    glDisable(GL_LIGHTING);
  }

  glutSwapBuffers();
  glutReportErrors();
}


////////////////////////////////////////////////////////////////////////////////
//! Keyboard events handler
////////////////////////////////////////////////////////////////////////////////
void keyboard(unsigned char key, int /*x*/, int /*y*/) {
  bool zoom = false;
  switch (key) {
    case (27):
      cleanup();
      exit(EXIT_SUCCESS);
      break;

    case 'w':
      wireframe = !wireframe;
      std::cout << "Wireframe: " << wireframe << std::endl;
      break;


    case 'l':
      lighting = !lighting;
      std::cout << "Lighting: " << lighting << std::endl;
      break;

    case 'r':
      render = !render;
      std::cout << "Render: " << render << std::endl;
      break;

    case 'a':
      _fovy -= 2.0;
      zoom=true;
      if (_fovy < 15.0) {_fovy = 15.0; zoom = false;}
      if (_fovy > 90.0) {_fovy = 90.0; zoom = false;}
      if(zoom){
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(_fovy, (float)window_width / (float)window_height, 0.1, 10.0);
        glMatrixMode(GL_MODELVIEW);
        glViewport(0, 0, window_width, window_height);
      }
      break;

    case 'd':
      _fovy += 2.0;
      zoom=true;
      if (_fovy < 15.0) {_fovy = 15.0; zoom = false;}
      if (_fovy > 90.0) {_fovy = 90.0; zoom = false;}
      if(zoom){
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(_fovy, (float)window_width / (float)window_height, 0.1, 10.0);
        glMatrixMode(GL_MODELVIEW);
        glViewport(0, 0, window_width, window_height);
      }

  // printf("isoValue = %f\n", isoValue);
  // printf("voxels = %d\n", activeVoxels);
  // printf("verts = %d\n", totalVerts);
  // printf("occupancy: %d / %d = %.2f%%\n", activeVoxels, numVoxels,
  //        activeVoxels * 100.0f / (float)numVoxels);

  }
}

void mouse(int button, int state, int x, int y) {
  if (state == GLUT_DOWN) {
    mouse_buttons |= 1 << button;
  } else if (state == GLUT_UP) {
    mouse_buttons = 0;
  }

  mouse_old_x = x;
  mouse_old_y = y;
}

void motion(int x, int y) {
  float dx = (float)(x - mouse_old_x);
  float dy = (float)(y - mouse_old_y);

  if (mouse_buttons == 1) {
    rotate.x += dy * 0.2f;
    rotate.y += dx * 0.2f;
  } else if (mouse_buttons == 2) {
    translate.x += dx * 0.01f;
    translate.y -= dy * 0.01f;
  } else if (mouse_buttons == 3) {
    translate.z += dy * 0.01f;
  }

  mouse_old_x = x;
  mouse_old_y = y;
  glutPostRedisplay();
}


void reshape(int w, int h) {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0, (float)w / (float)h, 0.1, 10.0);

  glMatrixMode(GL_MODELVIEW);
  glViewport(0, 0, w, h);
}

void timerEvent(int value) {
  glutPostRedisplay();
  glutTimerFunc(REFRESH_DELAY, timerEvent, 0);
}

void mainMenu(int i) { keyboard((unsigned char)i, 0, 0); }

void initMenus()
{
  glutCreateMenu(mainMenu);
  glutAddMenuEntry("Toggle rendering [r]", 'r');
  glutAddMenuEntry("Toggle lighting [l]", 'l');
  glutAddMenuEntry("Toggle wireframe [w]", 'w');
  glutAddMenuEntry("Exit [ESC]", 27);
  glutAttachMenu(GLUT_RIGHT_BUTTON);
}


mesh_t read_mesh(const std::string& filename)
{
  std::cout<< "Reading mesh data from file: " << filename << std::endl;;
  FILE* fp = fopen(filename.c_str(), "r");
  
  if (fp == NULL)
  {
    printf("Error: file not found\n");
    exit(-1);
  }
  
  int num_vertices, num_triangles;
  fscanf(fp, "%d %d\n", &num_vertices, &num_triangles);
  
  mesh_t mesh;
  mesh.num_vertices = num_vertices;
  mesh.num_triangles = num_triangles;
  std::cout << "[Input Data] Vertices: " << num_vertices << ", Triangles: " << num_triangles << std::endl;

  mesh.vertices = new float4[num_vertices];
  mesh.triangles = new uint3[num_triangles];

  for(int i = 0; i < num_vertices; i++)
  {
    float x, y, z;
    fscanf(fp, "v %f %f %f\n", &x, &y, &z);

    float4 vertex {x, y, z, 1.0f};
    mesh.vertices[i] = vertex;
  }
  for(int i = 0; i < num_triangles; i++)
  {
    uint v0, v1, v2;
    fscanf(fp, "f %u %u %u\n", &v0, &v1, &v2);

    uint3 face {v0, v1, v2};
    mesh.triangles[i] = face;
  }

  fclose(fp);
  std::cout<< "Done." << std::endl;
  return mesh;
}

void meshcopyHtoD(mesh_t mesh)
{
  size_t num_bytes;
  checkCudaErrors(cudaGraphicsMapResources(1, &cuda_posvbo_resource, 0));
  checkCudaErrors(cudaGraphicsResourceGetMappedPointer(
      (void **)&d_pos, &num_bytes, cuda_posvbo_resource));

  totalVerts = mesh.num_vertices;
  checkCudaErrors(cudaMemcpy(d_pos, mesh.vertices, mesh.num_vertices * sizeof(float4), cudaMemcpyHostToDevice));

  checkCudaErrors(cudaGraphicsUnmapResources(1, &cuda_posvbo_resource, 0));
}