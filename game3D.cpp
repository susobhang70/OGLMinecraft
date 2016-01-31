#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstdlib>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glad/glad.h>
#include <FTGL/ftgl.h>
#include <GLFW/glfw3.h>
#include <SOIL/SOIL.h>

#define YELLOW 1.0f, 1.0f, 0.0f

#define move_threshhold 1.2


using namespace std;

int defaultCam = 2;
int pits[895];
GLuint texturePlayer;

struct VAO {
    GLuint VertexArrayID;
    GLuint VertexBuffer;
    GLuint ColorBuffer;
	GLuint TextureBuffer;
	GLuint TextureID;

    GLenum PrimitiveMode;
    GLenum FillMode;
    int NumVertices;
};
typedef struct VAO VAO;

struct GLMatrices {
	glm::mat4 projection;
	glm::mat4 model;
	glm::mat4 view;
	GLuint MatrixID; // For use with normal shader
	GLuint TexMatrixID; // For use with texture shader
} Matrices;

struct FTGLFont {
	FTFont* font;
	GLuint fontMatrixID;
	GLuint fontColorID;
} GL3Font;

int compare (const void * a, const void * b)
{
  return ( *(int*)a - *(int*)b );
}

void findLavaBlocks()
{
	for(int i = 0; i < 895; i++)
		pits[i] = i+4;

	random_shuffle(&pits[0], &pits[895]);

	qsort(pits, 100, sizeof(int), compare);

	// for(int i = 0; i < 100; i++)
	// 	cout<<pits[i]<<endl;

}

GLuint programID, textureProgramID;

/* Function to load Shaders - Use it as it is */
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open())
	{
		std::string Line = "";
		while(getline(VertexShaderStream, Line))
			VertexShaderCode += "\n" + Line;
		VertexShaderStream.close();
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::string Line = "";
		while(getline(FragmentShaderStream, Line))
			FragmentShaderCode += "\n" + Line;
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> VertexShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &VertexShaderErrorMessage[0]);

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> FragmentShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &FragmentShaderErrorMessage[0]);

	// Link the program
	fprintf(stdout, "Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> ProgramErrorMessage( max(InfoLogLength, int(1)) );
	glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
	fprintf(stdout, "%s\n", &ProgramErrorMessage[0]);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

void quit(GLFWwindow *window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}

glm::vec3 getRGBfromHue (int hue)
{
	float intp;
	float fracp = modff(hue/60.0, &intp);
	float x = 1.0 - abs((float)((int)intp%2)+fracp-1.0);

	if (hue < 60)
		return glm::vec3(1,x,0);
	else if (hue < 120)
		return glm::vec3(x,1,0);
	else if (hue < 180)
		return glm::vec3(0,1,x);
	else if (hue < 240)
		return glm::vec3(0,x,1);
	else if (hue < 300)
		return glm::vec3(x,0,1);
	else
		return glm::vec3(1,0,x);
}


/* Generate VAO, VBOs and return VAO handle */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* color_buffer_data, GLenum fill_mode=GL_FILL)
{
    struct VAO* vao = new struct VAO;
    vao->PrimitiveMode = primitive_mode;
    vao->NumVertices = numVertices;
    vao->FillMode = fill_mode;

    // Create Vertex Array Object
    // Should be done after CreateWindow and before any other GL calls
    glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
    glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
    glGenBuffers (1, &(vao->ColorBuffer));  // VBO - colors

    glBindVertexArray (vao->VertexArrayID); // Bind the VAO 
    glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices 
    glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
    glVertexAttribPointer(
                          0,                  // attribute 0. Vertices
                          3,                  // size (x,y,z)
                          GL_FLOAT,           // type
                          GL_FALSE,           // normalized?
                          0,                  // stride
                          (void*)0            // array buffer offset
                          );

    glBindBuffer (GL_ARRAY_BUFFER, vao->ColorBuffer); // Bind the VBO colors 
    glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), color_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
    glVertexAttribPointer(
                          1,                  // attribute 1. Color
                          3,                  // size (r,g,b)
                          GL_FLOAT,           // type
                          GL_FALSE,           // normalized?
                          0,                  // stride
                          (void*)0            // array buffer offset
                          );

    return vao;
}

/* Generate VAO, VBOs and return VAO handle - Common Color for all vertices */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat red, const GLfloat green, const GLfloat blue, GLenum fill_mode=GL_FILL)
{
    GLfloat* color_buffer_data = new GLfloat [3*numVertices];
    for (int i=0; i<numVertices; i++) {
        color_buffer_data [3*i] = red;
        color_buffer_data [3*i + 1] = green;
        color_buffer_data [3*i + 2] = blue;
    }

    return create3DObject(primitive_mode, numVertices, vertex_buffer_data, color_buffer_data, fill_mode);
}

struct VAO* create3DTexturedObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* texture_buffer_data, GLuint textureID, GLenum fill_mode=GL_FILL)
{
	struct VAO* vao = new struct VAO;
	vao->PrimitiveMode = primitive_mode;
	vao->NumVertices = numVertices;
	vao->FillMode = fill_mode;
	vao->TextureID = textureID;

	// Create Vertex Array Object
	// Should be done after CreateWindow and before any other GL calls
	glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
	glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
	glGenBuffers (1, &(vao->TextureBuffer));  // VBO - textures

	glBindVertexArray (vao->VertexArrayID); // Bind the VAO
	glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
	glVertexAttribPointer(
						  0,                  // attribute 0. Vertices
						  3,                  // size (x,y,z)
						  GL_FLOAT,           // type
						  GL_FALSE,           // normalized?
						  0,                  // stride
						  (void*)0            // array buffer offset
						  );

	glBindBuffer (GL_ARRAY_BUFFER, vao->TextureBuffer); // Bind the VBO textures
	glBufferData (GL_ARRAY_BUFFER, 2*numVertices*sizeof(GLfloat), texture_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
	glVertexAttribPointer(
						  2,                  // attribute 2. Textures
						  2,                  // size (s,t)
						  GL_FLOAT,           // type
						  GL_FALSE,           // normalized?
						  0,                  // stride
						  (void*)0            // array buffer offset
						  );

	return vao;
}


/* Render the VBOs handled by VAO */
void draw3DObject (struct VAO* vao)
{
    // Change the Fill Mode for this object
    glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

    // Bind the VAO to use
    glBindVertexArray (vao->VertexArrayID);

    // Enable Vertex Attribute 0 - 3d Vertices
    glEnableVertexAttribArray(0);
    // Bind the VBO to use
    glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

    // Enable Vertex Attribute 1 - Color
    glEnableVertexAttribArray(1);
    // Bind the VBO to use
    glBindBuffer(GL_ARRAY_BUFFER, vao->ColorBuffer);

    // Draw the geometry !
    glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle
}

void draw3DTexturedObject (struct VAO* vao)
{
	// Change the Fill Mode for this object
	glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

	// Bind the VAO to use
	glBindVertexArray (vao->VertexArrayID);

	// Enable Vertex Attribute 0 - 3d Vertices
	glEnableVertexAttribArray(0);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

	// Bind Textures using texture units
	glBindTexture(GL_TEXTURE_2D, vao->TextureID);

	// Enable Vertex Attribute 2 - Texture
	glEnableVertexAttribArray(2);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->TextureBuffer);

	// Draw the geometry !
	glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle

	// Unbind Textures to be safe
	glBindTexture(GL_TEXTURE_2D, 0);
}

/* Create an OpenGL Texture from an image */
GLuint createTexture (const char* filename)
{
	GLuint TextureID;
	// Generate Texture Buffer
	glGenTextures(1, &TextureID);
	// All upcoming GL_TEXTURE_2D operations now have effect on our texture buffer
	glBindTexture(GL_TEXTURE_2D, TextureID);
	// Set our texture parameters
	// Set texture wrapping to GL_REPEAT
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Set texture filtering (interpolation)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Load image and create OpenGL texture
	int twidth, theight;
	unsigned char* image = SOIL_load_image(filename, &twidth, &theight, 0, SOIL_LOAD_RGB);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, twidth, theight, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D); // Generate MipMaps to use
	SOIL_free_image_data(image); // Free the data read from file after creating opengl texture
	glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture when done, so we won't accidentily mess it up

	return TextureID;
}


/**************************
 * Customizable functions *
 **************************/

float triangle_rot_dir = 1;
float rectangle_rot_dir = 1;
bool triangle_rot_status = true;
bool rectangle_rot_status = true;


/* Executed when window is resized to 'width' and 'height' */
/* Modify the bounds of the screen here in glm::ortho or Field of View in glm::Perspective */
void reshapeWindow (GLFWwindow* window, int width, int height)
{
    int fbwidth=width, fbheight=height;
    /* With Retina display on Mac OS X, GLFW's FramebufferSize
     is different from WindowSize */
    glfwGetFramebufferSize(window, &fbwidth, &fbheight);

	GLfloat fov = 90.0f;

	// sets the viewport of openGL renderer
	glViewport (0, 0, (GLsizei) fbwidth, (GLsizei) fbheight);

	// set the projection matrix as perspective
	// glMatrixMode (GL_PROJECTION);
	 //  glLoadIdentity ();
	  // gluPerspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1, 500.0); 
	// Store the projection matrix in a variable for future use
    // Perspective projection for 3D views
     Matrices.projection = glm::perspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1f, 500.0f);

    // Ortho projection for 2D views
  // Matrices.projection = glm::ortho(-4.0f, 4.0f, -4.0f, 4.0f, 0.1f, 500.0f);
}

VAO *triangle, *rectangle, *cube;

// Creates the triangle object used in this sample code
void createTriangle ()
{
  /* ONLY vertices between the bounds specified in glm::ortho will be visible on screen */

  /* Define vertex array as used in glBegin (GL_TRIANGLES) */
  static const GLfloat vertex_buffer_data [] = {
    0, 1,0, // vertex 0
    -1,-1,0, // vertex 1
    1,-1,0, // vertex 2
  };

  static const GLfloat color_buffer_data [] = {
    1,0,0, // color 0
    0,1,0, // color 1
    0,0,1, // color 2
  };

  // create3DObject creates and returns a handle to a VAO that can be used later
  triangle = create3DObject(GL_TRIANGLES, 3, vertex_buffer_data, color_buffer_data, GL_LINE);
}

// Creates the rectangle object used in this sample code
void createRectangle ()
{
  // GL3 accepts only Triangles. Quads are not supported
  static const GLfloat vertex_buffer_data [] = {
    -1.2,-1,0, // vertex 1
    1.2,-1,0, // vertex 2
    1.2, 1,0, // vertex 3

    1.2, 1,0, // vertex 3
    -1.2, 1,0, // vertex 4
    -1.2,-1,0  // vertex 1
  };

  static const GLfloat color_buffer_data [] = {
    1,0,0, // color 1
    0,0,1, // color 2
    0,1,0, // color 3

    0,1,0, // color 3
    0.3,0.3,0.3, // color 4
    1,0,0  // color 1
  };

  // create3DObject creates and returns a handle to a VAO that can be used later
  rectangle = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

class Cuboid
{
	protected:
		float m_x, m_y, m_z, m_rotation;
		float m_height, m_width, m_length;
		GLfloat m_vertex_buffer_data[12*3*3];
		GLfloat m_color_buffer_data[12*3*3];
		GLfloat m_texture_buffer_data[12*3*2];
		VAO *m_cuboid;
		GLuint m_textureID;
		int m_blocktype;

	public:
		Cuboid(float x, float y, float z, float length, float width, float height);
		Cuboid(Cuboid &c);
		void createCuboid(GLuint, int);
		void draw();
		void grassBlock();
		void waterBlock();
		float getX() { return m_x; }
		float getY() { return m_y; }
		float getZ() { return m_z; }
		int getBlockType() { return m_blocktype; }
};

Cuboid::Cuboid(float x, float y, float z, float length, float width, float height)
{
	m_x = x;
	m_y = y;
	m_z = z;
	m_length = length;
	m_width = width;
	m_height = height;
	m_rotation = 0;
	m_cuboid = NULL;

}

Cuboid::Cuboid(Cuboid &c)
{
	m_x = c.m_x;
	m_y = c.m_y;
	m_z = c.m_z;
	m_length = c.m_length;
	m_width = c.m_width;
	m_height = c.m_height;
	m_cuboid = c.m_cuboid;
	m_rotation = c.m_rotation;
}

void Cuboid::createCuboid(GLuint textureID, int blocktype)
{
	m_textureID = textureID;
	m_blocktype = blocktype;

	// Face 1 traingle 1
	m_vertex_buffer_data[0] = 0;
	m_vertex_buffer_data[1] = 0;
	m_vertex_buffer_data[2] = 0;

	m_vertex_buffer_data[3] = 0;
	m_vertex_buffer_data[4] = -m_length;
	m_vertex_buffer_data[5] = 0;

	m_vertex_buffer_data[6] = m_width;
	m_vertex_buffer_data[7] = -m_length;
	m_vertex_buffer_data[8] = 0;

	// Face 1 traingle 2

	m_vertex_buffer_data[9] = 0;
	m_vertex_buffer_data[10] = 0;
	m_vertex_buffer_data[11] = 0;
	
	m_vertex_buffer_data[12] = m_width;
	m_vertex_buffer_data[13] = 0;
	m_vertex_buffer_data[14] = 0;
	
	m_vertex_buffer_data[15] = m_width;
	m_vertex_buffer_data[16] = -m_length;
	m_vertex_buffer_data[17] = 0;

	// Face 2 triangle 1
	
	m_vertex_buffer_data[18] = m_width;
	m_vertex_buffer_data[19] = 0;
	m_vertex_buffer_data[20] = 0;
	
	m_vertex_buffer_data[21] = m_width;
	m_vertex_buffer_data[22] = -m_length;
	m_vertex_buffer_data[23] = 0;
	
	m_vertex_buffer_data[24] = m_width;
	m_vertex_buffer_data[25] = -m_length;
	m_vertex_buffer_data[26] = -m_height;
	
	// Face 2 triangle 2

	m_vertex_buffer_data[27] = m_width;
	m_vertex_buffer_data[28] = 0;
	m_vertex_buffer_data[29] = 0;
	
	m_vertex_buffer_data[30] = m_width;
	m_vertex_buffer_data[31] = 0;
	m_vertex_buffer_data[32] = -m_height;
	
	m_vertex_buffer_data[33] = m_width;
	m_vertex_buffer_data[34] = -m_length;
	m_vertex_buffer_data[35] = -m_height;

	// Face 3 triangle 1
	
	m_vertex_buffer_data[36] = m_width;
	m_vertex_buffer_data[37] = 0;
	m_vertex_buffer_data[38] = -m_height;
	
	m_vertex_buffer_data[39] = m_width;
	m_vertex_buffer_data[40] = -m_length;
	m_vertex_buffer_data[41] = -m_height;
	
	m_vertex_buffer_data[42] = 0;
	m_vertex_buffer_data[43] = -m_length;
	m_vertex_buffer_data[44] = -m_height;

	//Face 3 triangle 2
	
	m_vertex_buffer_data[45] = m_width;
	m_vertex_buffer_data[46] = 0;
	m_vertex_buffer_data[47] = -m_height;

	m_vertex_buffer_data[48] = 0;
	m_vertex_buffer_data[49] = 0;
	m_vertex_buffer_data[50] = -m_height;

	m_vertex_buffer_data[51] = 0;
	m_vertex_buffer_data[52] = -m_length;
	m_vertex_buffer_data[53] = -m_height;

	// Face 4 triangle 1

	m_vertex_buffer_data[54] = 0;
	m_vertex_buffer_data[55] = 0;
	m_vertex_buffer_data[56] = -m_height;

	m_vertex_buffer_data[57] = 0;
	m_vertex_buffer_data[58] = -m_length;
	m_vertex_buffer_data[59] = -m_height;

	m_vertex_buffer_data[60] = 0;
	m_vertex_buffer_data[61] = -m_length;
	m_vertex_buffer_data[62] = 0;

	// Face 4 triangle 2

	m_vertex_buffer_data[63] = 0;
	m_vertex_buffer_data[64] = 0;
	m_vertex_buffer_data[65] = -m_height;

	m_vertex_buffer_data[66] = 0;
	m_vertex_buffer_data[67] = 0;
	m_vertex_buffer_data[68] = 0;

	m_vertex_buffer_data[69] = 0;
	m_vertex_buffer_data[70] = -m_length;
	m_vertex_buffer_data[71] = 0;

	// Face 5 triangle 1

	m_vertex_buffer_data[72] = 0;
	m_vertex_buffer_data[73] = 0;
	m_vertex_buffer_data[74] = -m_height;

	m_vertex_buffer_data[75] = 0;
	m_vertex_buffer_data[76] = 0;
	m_vertex_buffer_data[77] = 0;

	m_vertex_buffer_data[78] = m_width;
	m_vertex_buffer_data[79] = 0;
	m_vertex_buffer_data[80] = 0;

	// Face 5 triangle 2

	m_vertex_buffer_data[81] = 0;
	m_vertex_buffer_data[82] = 0;
	m_vertex_buffer_data[83] = -m_height;

	m_vertex_buffer_data[84] = m_width;
	m_vertex_buffer_data[85] = 0;
	m_vertex_buffer_data[86] = -m_height;

	m_vertex_buffer_data[87] = m_width;
	m_vertex_buffer_data[88] = 0;
	m_vertex_buffer_data[89] = 0;

	//Face 6 triangle 1

	m_vertex_buffer_data[90] = 0;
	m_vertex_buffer_data[91] = -m_length;
	m_vertex_buffer_data[92] = 0;

	m_vertex_buffer_data[93] = 0;
	m_vertex_buffer_data[94] = -m_length;
	m_vertex_buffer_data[95] = -m_height;

	m_vertex_buffer_data[96] = m_width;
	m_vertex_buffer_data[97] = -m_length;
	m_vertex_buffer_data[98] = -m_height;

	// Face 6 triangle 2

	m_vertex_buffer_data[99] = 0;
	m_vertex_buffer_data[100] = -m_length;
	m_vertex_buffer_data[101] = 0;

	m_vertex_buffer_data[102] = m_width;
	m_vertex_buffer_data[103] = -m_length;
	m_vertex_buffer_data[104] = 0;

	m_vertex_buffer_data[105] = m_width;
	m_vertex_buffer_data[106] = -m_length;
	m_vertex_buffer_data[107] = -m_height;


	if(blocktype == 1 || blocktype == 4 || blocktype == 5)
		grassBlock();
	else if(blocktype == 2)
		waterBlock();
	else if(blocktype == 3)
		return; 
 	m_cuboid = create3DTexturedObject(GL_TRIANGLES, 12 * 3, m_vertex_buffer_data, m_texture_buffer_data, m_textureID, GL_FILL);
}

void Cuboid::grassBlock()
{

	m_texture_buffer_data[0] = 0.25;
	m_texture_buffer_data[1] = 0.34;
	m_texture_buffer_data[2] = 0.25;
	m_texture_buffer_data[3] = 0.66;
	m_texture_buffer_data[4] = 0.5;
	m_texture_buffer_data[5] = 0.66;

	m_texture_buffer_data[6] = 0.25;
	m_texture_buffer_data[7] = 0.34;
	m_texture_buffer_data[8] = 0.5;
	m_texture_buffer_data[9] = 0.34;
	m_texture_buffer_data[10] = 0.5;
	m_texture_buffer_data[11] = 0.66;


	m_texture_buffer_data[12] = 0.5;
	m_texture_buffer_data[13] = 0.34;
	m_texture_buffer_data[14] = 0.5;
	m_texture_buffer_data[15] = 0.66;
	m_texture_buffer_data[16] = 0.75;
	m_texture_buffer_data[17] = 0.66;

	m_texture_buffer_data[18] = 0.5;
	m_texture_buffer_data[19] = 0.34;
	m_texture_buffer_data[20] = 0.75;
	m_texture_buffer_data[21] = 0.34;
	m_texture_buffer_data[22] = 0.75;
	m_texture_buffer_data[23] = 0.66;


	m_texture_buffer_data[24] = 0.75;
	m_texture_buffer_data[25] = 0.34;
	m_texture_buffer_data[26] = 0.75;
	m_texture_buffer_data[27] = 0.66;
	m_texture_buffer_data[28] = 1;
	m_texture_buffer_data[29] = 0.66;
	m_texture_buffer_data[30] = 0.75;
	m_texture_buffer_data[31] = 0.34;
	m_texture_buffer_data[32] = 1;
	m_texture_buffer_data[33] = 0.34;
	m_texture_buffer_data[34] = 1;
	m_texture_buffer_data[35] = 0.66;


	m_texture_buffer_data[36] = 0;
	m_texture_buffer_data[37] = 0.34;
	m_texture_buffer_data[38] = 0;
	m_texture_buffer_data[39] = 0.66;
	m_texture_buffer_data[40] = 0.25;
	m_texture_buffer_data[41] = 0.66;
	m_texture_buffer_data[42] = 0;
	m_texture_buffer_data[43] = 0.34;
	m_texture_buffer_data[44] = 0.25;
	m_texture_buffer_data[45] = 0.34;
	m_texture_buffer_data[46] = 0.25;
	m_texture_buffer_data[47] = 0.66;

 	// Face 5 #24 #27
 	m_texture_buffer_data[48] = 0.25;
 	m_texture_buffer_data[49] = 0;
 	m_texture_buffer_data[54] = 0.25;
 	m_texture_buffer_data[55] = 0;

 	// Face 5 #25
 	m_texture_buffer_data[50] = 0.25;
 	m_texture_buffer_data[51] = 0.33;

 	// Face 5 #26 #29
 	m_texture_buffer_data[52] = 0.5;
 	m_texture_buffer_data[53] = 0.33;
 	m_texture_buffer_data[58] = 0.5;
 	m_texture_buffer_data[59] = 0.33;

 	// Face 5 #28
 	m_texture_buffer_data[56] = 0.5;
 	m_texture_buffer_data[57] = 0;

 	
	m_texture_buffer_data[60] = 0.25;
	m_texture_buffer_data[61] = 0.67;
	m_texture_buffer_data[62] = 0.25;
	m_texture_buffer_data[63] = 1;
	m_texture_buffer_data[64] = 0.5;
	m_texture_buffer_data[65] = 1;

	m_texture_buffer_data[66] = 0.25;
	m_texture_buffer_data[67] = 0.67;
	m_texture_buffer_data[68] = 0.5;
	m_texture_buffer_data[69] = 0.67;
	m_texture_buffer_data[70] = 0.5;
	m_texture_buffer_data[71] = 1;
}

void Cuboid::waterBlock()
{

	for(int v = 0; v < 36; v++)
	{
		m_texture_buffer_data[2*v + 0] = 1;
		m_texture_buffer_data[2*v + 1] = 1;
	}

	// Face 5 #24 #27
 	m_texture_buffer_data[48] = 0;
 	m_texture_buffer_data[49] = 0;
 	m_texture_buffer_data[54] = 0;
 	m_texture_buffer_data[55] = 0;

 	// Face 5 #25
 	m_texture_buffer_data[50] = 0;
 	m_texture_buffer_data[51] = 1;

 	// Face 5 #26 #29
 	m_texture_buffer_data[52] = 1;
 	m_texture_buffer_data[53] = 1;
 	m_texture_buffer_data[58] = 1;
 	m_texture_buffer_data[59] = 1;

 	// Face 5 #28
 	m_texture_buffer_data[56] = 1;
 	m_texture_buffer_data[57] = 0;

	return;
}

float camera_rotation_angle = 90;
float rectangle_rotation = 0;
float triangle_rotation = 0;

/* Render the scene with openGL */
/* Edit this function according to your assignment */
void Cuboid::draw ()
{

  // Send our transformation to the currently bound shader, in the "MVP" uniform
  // For each model you render, since the MVP will be different (at least the M part)
  //  Don't change unless you are sure!!
  glm::mat4 MVP;	// MVP = Projection * View * Model


  // Load identity to model matrix
  Matrices.model = glm::mat4(1.0f);

  /* Render your scene */
/*
  glm::mat4 translateTriangle = glm::translate (glm::vec3(-2.0f, 0.0f, 0.0f)); // glTranslatef
  glm::mat4 rotateTriangle = glm::rotate((float)(triangle_rotation*M_PI/180.0f), glm::vec3(0,0,1));  // rotate about vector (1,0,0)
  glm::mat4 triangleTransform = translateTriangle * rotateTriangle;
  Matrices.model *= triangleTransform; 
  MVP = VP * Matrices.model; // MVP = p * V * M

  //  Don't change unless you are sure!!
  glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

  // draw3DObject draws the VAO given to it using current MVP matrix
  draw3DObject(triangle);

  // Pop matrix to undo transformations till last push matrix instead of recomputing model matrix
  // glPopMatrix ();
  Matrices.model = glm::mat4(1.0f);

  glm::mat4 translateRectangle = glm::translate (glm::vec3(2, 0, 0));        // glTranslatef
  glm::mat4 rotateRectangle = glm::rotate((float)(rectangle_rotation*M_PI/180.0f), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)
  Matrices.model *= (translateRectangle * rotateRectangle);
  MVP = VP * Matrices.model;
  glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

  // draw3DObject draws the VAO given to it using current MVP matrix
  draw3DObject(rectangle);
*/

  Matrices.model = glm::mat4(1.0f);
  glm::mat4 translateCuboid = glm::translate (glm::vec3(m_x, m_y, m_z));  
  glm::mat4 rotateCuboid = glm::rotate((float)(m_rotation*M_PI/180.0f), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)
  Matrices.model *= (translateCuboid * rotateCuboid);
  MVP = Matrices.projection * Matrices.view* Matrices.model;

  // glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
  // draw3DObject(m_cuboid);


  glUniformMatrix4fv(Matrices.TexMatrixID, 1, GL_FALSE, &MVP[0][0]);

	// Set the texture sampler to access Texture0 memory
	glUniform1i(glGetUniformLocation(textureProgramID, "texSampler"), 0);

	// draw3DObject draws the VAO given to it using current MVP matrix
	draw3DTexturedObject(m_cuboid);

  // Increment angles
  // float increments = 1;

  // camera_rotation_angle += 0.01; // Simulating camera rotation
  // triangle_rotation = triangle_rotation + increments*triangle_rot_dir*triangle_rot_status;
  // rectangle_rotation = rectangle_rotation + increments*rectangle_rot_dir*rectangle_rot_status;
}

class Player : public Cuboid
{
	private:
		int m_type;
		float m_totalmove, m_rotationincrement;

	public:
		Player(float x, float y, float z, float length, float width, float height);
		Player(Player &p);
		void createPlayer(GLuint texturePlayer, int part);
		void moveForward();
		void moveBackward();
		void moveLeft();
		void moveRight();
		void playerHeadBlock();
		void playerBodyBlock();
		void playerLegBlock();
		void playerRightHandBlock();
		void playerLeftHandBlock();
};

Player::Player(float x, float y, float z, float length, float width, float height): Cuboid(x, y, z, length, width, height)
{
	m_type = 0;
}

Player::Player(Player &p): Cuboid(p)
{
	m_type = p.m_type;
}

void Player::createPlayer(GLuint texturePlayer, int part)
{
	createCuboid(texturePlayer, 3);
	m_type = part;
	if(part == 1)
		playerHeadBlock();
	else if(part == 2)
		playerBodyBlock();
	else if(part == 3)
	{
		playerLegBlock();
		m_rotationincrement = 5.0;
	}
	else if(part == 4)
	{
		playerLeftHandBlock();
		m_rotationincrement = 5.0;
	}
	else if(part == 5)
	{
		playerRightHandBlock();
		m_rotationincrement = -5.0;
	}
	else if(part == 6)
	{
		playerLegBlock();
		m_rotationincrement = -5.0;
	}
	return;
}

void Player::playerHeadBlock()
{
	for(int v = 0; v < 36; v++)
	{
		m_texture_buffer_data[2*v + 0] = 44.0/420.0;
		m_texture_buffer_data[2*v + 1] = 4.0/224.0;
	}

	int offset = 12;

 	m_texture_buffer_data[12] = m_texture_buffer_data[18] = m_texture_buffer_data[14] = 28.0/420.0;
 	m_texture_buffer_data[13] = m_texture_buffer_data[19] = m_texture_buffer_data[21] = 0;
 	m_texture_buffer_data[20] = m_texture_buffer_data[16] = m_texture_buffer_data[22] = 84.0/420.0;
	m_texture_buffer_data[15] = m_texture_buffer_data[17] = m_texture_buffer_data[23] = 56.0/224.0;

	m_texture_buffer_data[12+offset] = m_texture_buffer_data[18+offset] = m_texture_buffer_data[14+offset] = 364.0/420.0;
 	m_texture_buffer_data[13+offset] = m_texture_buffer_data[19+offset] = m_texture_buffer_data[21+offset] = 0;
 	m_texture_buffer_data[20+offset] = m_texture_buffer_data[16+offset] = m_texture_buffer_data[22+offset] = 420.0/420.0;
	m_texture_buffer_data[15+offset] = m_texture_buffer_data[17+offset] = m_texture_buffer_data[23+offset] = 56.0/224.0;

	m_texture_buffer_data[12-offset] = m_texture_buffer_data[18-offset] = m_texture_buffer_data[14-offset] = 140.0/420.0;
 	m_texture_buffer_data[13-offset] = m_texture_buffer_data[19-offset] = m_texture_buffer_data[21-offset] = 0;
 	m_texture_buffer_data[20-offset] = m_texture_buffer_data[16-offset] = m_texture_buffer_data[22-offset] = 196.0/420.0;
	m_texture_buffer_data[15-offset] = m_texture_buffer_data[17-offset] = m_texture_buffer_data[23-offset] = 56.0/224.0;

	m_texture_buffer_data[12+(2*offset)] = m_texture_buffer_data[18+(2*offset)] = m_texture_buffer_data[14+(2*offset)] = 252.0/420.0;
 	m_texture_buffer_data[13+(2*offset)] = m_texture_buffer_data[19+(2*offset)] = m_texture_buffer_data[21+(2*offset)] = 0;
 	m_texture_buffer_data[20+(2*offset)] = m_texture_buffer_data[16+(2*offset)] = m_texture_buffer_data[22+(2*offset)] = 308.0/420.0;
	m_texture_buffer_data[15+(2*offset)] = m_texture_buffer_data[17+(2*offset)] = m_texture_buffer_data[23+(2*offset)] = 56.0/224.0;

 	m_cuboid = create3DTexturedObject(GL_TRIANGLES, 12 * 3, m_vertex_buffer_data, m_texture_buffer_data, m_textureID, GL_FILL);

	return;
}

void Player::playerBodyBlock()
{
	for(int v = 0; v < 36; v++)
	{
		m_texture_buffer_data[2*v + 0] = 0;
		m_texture_buffer_data[2*v + 1] = 0;
	}

	int offset = 12;

 	m_texture_buffer_data[12] = m_texture_buffer_data[18] = m_texture_buffer_data[14] = 28.0/420.0;
 	m_texture_buffer_data[13] = m_texture_buffer_data[19] = m_texture_buffer_data[21] = 56.0/224.0;
 	m_texture_buffer_data[20] = m_texture_buffer_data[16] = m_texture_buffer_data[22] = 84.0/420.0;
	m_texture_buffer_data[15] = m_texture_buffer_data[17] = m_texture_buffer_data[23] = 140.0/224.0;

	m_texture_buffer_data[12+offset] = m_texture_buffer_data[18+offset] = m_texture_buffer_data[14+offset] = 378.0/420.0;
 	m_texture_buffer_data[13+offset] = m_texture_buffer_data[19+offset] = m_texture_buffer_data[21+offset] = 56.0/224.0;
 	m_texture_buffer_data[20+offset] = m_texture_buffer_data[16+offset] = m_texture_buffer_data[22+offset] = 406.0/420.0;
	m_texture_buffer_data[15+offset] = m_texture_buffer_data[17+offset] = m_texture_buffer_data[23+offset] = 140.0/224.0;

	m_texture_buffer_data[12-offset] = m_texture_buffer_data[18-offset] = m_texture_buffer_data[14-offset] = 154.0/420.0;
 	m_texture_buffer_data[13-offset] = m_texture_buffer_data[19-offset] = m_texture_buffer_data[21-offset] = 56.0/224.0;
 	m_texture_buffer_data[20-offset] = m_texture_buffer_data[16-offset] = m_texture_buffer_data[22-offset] = 182.0/420.0;
	m_texture_buffer_data[15-offset] = m_texture_buffer_data[17-offset] = m_texture_buffer_data[23-offset] = 140.0/224.0;

	m_texture_buffer_data[12+(2*offset)] = m_texture_buffer_data[18+(2*offset)] = m_texture_buffer_data[14+(2*offset)] = 252.0/420.0;
 	m_texture_buffer_data[13+(2*offset)] = m_texture_buffer_data[19+(2*offset)] = m_texture_buffer_data[21+(2*offset)] = 56.0/224.0;
 	m_texture_buffer_data[20+(2*offset)] = m_texture_buffer_data[16+(2*offset)] = m_texture_buffer_data[22+(2*offset)] = 308.0/420.0;
	m_texture_buffer_data[15+(2*offset)] = m_texture_buffer_data[17+(2*offset)] = m_texture_buffer_data[23+(2*offset)] = 140.0/224.0;

 	m_cuboid = create3DTexturedObject(GL_TRIANGLES, 12 * 3, m_vertex_buffer_data, m_texture_buffer_data, m_textureID, GL_FILL);

}

void Player::playerLegBlock()
{
	for(int v = 0; v < 36; v++)
	{
		m_texture_buffer_data[2*v + 0] = 0;
		m_texture_buffer_data[2*v + 1] = 0;
	}

	int offset = 12;

 	m_texture_buffer_data[12] = m_texture_buffer_data[18] = m_texture_buffer_data[14] = 28.0/420.0;
 	m_texture_buffer_data[13] = m_texture_buffer_data[19] = m_texture_buffer_data[21] = 140.0/224.0;
 	m_texture_buffer_data[20] = m_texture_buffer_data[16] = m_texture_buffer_data[22] = 84.0/420.0;
	m_texture_buffer_data[15] = m_texture_buffer_data[17] = m_texture_buffer_data[23] = 224.0/224.0;

	m_texture_buffer_data[12+offset] = m_texture_buffer_data[18+offset] = m_texture_buffer_data[14+offset] = 378.0/420.0;
 	m_texture_buffer_data[13+offset] = m_texture_buffer_data[19+offset] = m_texture_buffer_data[21+offset] = 140.0/224.0;
 	m_texture_buffer_data[20+offset] = m_texture_buffer_data[16+offset] = m_texture_buffer_data[22+offset] = 406.0/420.0;
	m_texture_buffer_data[15+offset] = m_texture_buffer_data[17+offset] = m_texture_buffer_data[23+offset] = 224.0/224.0;

	m_texture_buffer_data[12-offset] = m_texture_buffer_data[18-offset] = m_texture_buffer_data[14-offset] = 154.0/420.0;
 	m_texture_buffer_data[13-offset] = m_texture_buffer_data[19-offset] = m_texture_buffer_data[21-offset] = 140.0/224.0;
 	m_texture_buffer_data[20-offset] = m_texture_buffer_data[16-offset] = m_texture_buffer_data[22-offset] = 182.0/420.0;
	m_texture_buffer_data[15-offset] = m_texture_buffer_data[17-offset] = m_texture_buffer_data[23-offset] = 224.0/224.0;

	m_texture_buffer_data[12+(2*offset)] = m_texture_buffer_data[18+(2*offset)] = m_texture_buffer_data[14+(2*offset)] = 252.0/420.0;
 	m_texture_buffer_data[13+(2*offset)] = m_texture_buffer_data[19+(2*offset)] = m_texture_buffer_data[21+(2*offset)] = 140.0/224.0;
 	m_texture_buffer_data[20+(2*offset)] = m_texture_buffer_data[16+(2*offset)] = m_texture_buffer_data[22+(2*offset)] = 308.0/420.0;
	m_texture_buffer_data[15+(2*offset)] = m_texture_buffer_data[17+(2*offset)] = m_texture_buffer_data[23+(2*offset)] = 224.0/224.0;

 	m_cuboid = create3DTexturedObject(GL_TRIANGLES, 12 * 3, m_vertex_buffer_data, m_texture_buffer_data, m_textureID, GL_FILL);
}

void Player::playerRightHandBlock()
{
	for(int v = 0; v < 36; v++)
	{
		m_texture_buffer_data[2*v + 0] = 0;
		m_texture_buffer_data[2*v + 1] = 0;
	}

	int offset = 12;

 	m_texture_buffer_data[12] = m_texture_buffer_data[18] = m_texture_buffer_data[14] = 0.0/420.0;
 	m_texture_buffer_data[13] = m_texture_buffer_data[19] = m_texture_buffer_data[21] = 56.0/224.0;
 	m_texture_buffer_data[20] = m_texture_buffer_data[16] = m_texture_buffer_data[22] = 28.0/420.0;
	m_texture_buffer_data[15] = m_texture_buffer_data[17] = m_texture_buffer_data[23] = 140.0/224.0;

	m_texture_buffer_data[12+offset] = m_texture_buffer_data[18+offset] = m_texture_buffer_data[14+offset] = 378.0/420.0;
 	m_texture_buffer_data[13+offset] = m_texture_buffer_data[19+offset] = m_texture_buffer_data[21+offset] = 56.0/224.0;
 	m_texture_buffer_data[20+offset] = m_texture_buffer_data[16+offset] = m_texture_buffer_data[22+offset] = 406.0/420.0;
	m_texture_buffer_data[15+offset] = m_texture_buffer_data[17+offset] = m_texture_buffer_data[23+offset] = 140.0/224.0;

	m_texture_buffer_data[12-offset] = m_texture_buffer_data[18-offset] = m_texture_buffer_data[14-offset] = 154.0/420.0;
 	m_texture_buffer_data[13-offset] = m_texture_buffer_data[19-offset] = m_texture_buffer_data[21-offset] = 56.0/224.0;
 	m_texture_buffer_data[20-offset] = m_texture_buffer_data[16-offset] = m_texture_buffer_data[22-offset] = 182.0/420.0;
	m_texture_buffer_data[15-offset] = m_texture_buffer_data[17-offset] = m_texture_buffer_data[23-offset] = 140.0/224.0;

	m_texture_buffer_data[12+(2*offset)] = m_texture_buffer_data[18+(2*offset)] = m_texture_buffer_data[14+(2*offset)] = 308.0/420.0;
 	m_texture_buffer_data[13+(2*offset)] = m_texture_buffer_data[19+(2*offset)] = m_texture_buffer_data[21+(2*offset)] = 56.0/224.0;
 	m_texture_buffer_data[20+(2*offset)] = m_texture_buffer_data[16+(2*offset)] = m_texture_buffer_data[22+(2*offset)] = 336.0/420.0;
	m_texture_buffer_data[15+(2*offset)] = m_texture_buffer_data[17+(2*offset)] = m_texture_buffer_data[23+(2*offset)] = 140.0/224.0;

 	m_cuboid = create3DTexturedObject(GL_TRIANGLES, 12 * 3, m_vertex_buffer_data, m_texture_buffer_data, m_textureID, GL_FILL);
}

void Player::playerLeftHandBlock()
{
	for(int v = 0; v < 36; v++)
	{
		m_texture_buffer_data[2*v + 0] = 0;
		m_texture_buffer_data[2*v + 1] = 0;
	}

	int offset = 12;

 	m_texture_buffer_data[12] = m_texture_buffer_data[18] = m_texture_buffer_data[14] = 84.0/420.0;
 	m_texture_buffer_data[13] = m_texture_buffer_data[19] = m_texture_buffer_data[21] = 56.0/224.0;
 	m_texture_buffer_data[20] = m_texture_buffer_data[16] = m_texture_buffer_data[22] = 112.0/420.0;
	m_texture_buffer_data[15] = m_texture_buffer_data[17] = m_texture_buffer_data[23] = 140.0/224.0;

	m_texture_buffer_data[12+offset] = m_texture_buffer_data[18+offset] = m_texture_buffer_data[14+offset] = 378.0/420.0;
 	m_texture_buffer_data[13+offset] = m_texture_buffer_data[19+offset] = m_texture_buffer_data[21+offset] = 56.0/224.0;
 	m_texture_buffer_data[20+offset] = m_texture_buffer_data[16+offset] = m_texture_buffer_data[22+offset] = 406.0/420.0;
	m_texture_buffer_data[15+offset] = m_texture_buffer_data[17+offset] = m_texture_buffer_data[23+offset] = 140.0/224.0;

	m_texture_buffer_data[12-offset] = m_texture_buffer_data[18-offset] = m_texture_buffer_data[14-offset] = 154.0/420.0;
 	m_texture_buffer_data[13-offset] = m_texture_buffer_data[19-offset] = m_texture_buffer_data[21-offset] = 56.0/224.0;
 	m_texture_buffer_data[20-offset] = m_texture_buffer_data[16-offset] = m_texture_buffer_data[22-offset] = 182.0/420.0;
	m_texture_buffer_data[15-offset] = m_texture_buffer_data[17-offset] = m_texture_buffer_data[23-offset] = 140.0/224.0;

	m_texture_buffer_data[12+(2*offset)] = m_texture_buffer_data[18+(2*offset)] = m_texture_buffer_data[14+(2*offset)] = 224.0/420.0;
 	m_texture_buffer_data[13+(2*offset)] = m_texture_buffer_data[19+(2*offset)] = m_texture_buffer_data[21+(2*offset)] = 56.0/224.0;
 	m_texture_buffer_data[20+(2*offset)] = m_texture_buffer_data[16+(2*offset)] = m_texture_buffer_data[22+(2*offset)] = 252.0/420.0;
	m_texture_buffer_data[15+(2*offset)] = m_texture_buffer_data[17+(2*offset)] = m_texture_buffer_data[23+(2*offset)] = 140.0/224.0;

 	m_cuboid = create3DTexturedObject(GL_TRIANGLES, 12 * 3, m_vertex_buffer_data, m_texture_buffer_data, m_textureID, GL_FILL);
}

void Player::moveForward()
{
	m_x += 0.1;
	if(m_type == 4 || m_type == 5 || m_type == 3 || m_type == 6)
	{
		m_rotation += m_rotationincrement;
		if(m_rotation > 30.0f)
		{
			m_rotationincrement *= -1;
			m_rotation = 30.0f;
		}
		else if(m_rotation < -30.0f)
		{
			m_rotationincrement *= -1;
			m_rotation = -30.0f;
		}
	}
}

void Player::moveBackward()
{
	m_x -= 0.1;
	if(m_type == 4 || m_type == 5 || m_type == 3 || m_type == 6)
	{
		m_rotation += m_rotationincrement;
		if(m_rotation > 30.0f)
		{
			m_rotationincrement *= -1;
			m_rotation = 30.0f;
		}
		else if(m_rotation < -30.0f)
		{
			m_rotationincrement *= -1;
			m_rotation = -30.0f;
		}
	}
}

void Player::moveLeft()
{
	m_z -= 0.1;
	if(m_type == 4 || m_type == 5 || m_type == 3 || m_type == 6)
	{
		m_rotation += m_rotationincrement;
		if(m_rotation > 30.0f)
		{
			m_rotationincrement *= -1;
			m_rotation = 30.0f;
		}
		else if(m_rotation < -30.0f)
		{
			m_rotationincrement *= -1;
			m_rotation = -30.0f;
		}
	}
}

void Player::moveRight()
{
	m_z += 0.1;
	if(m_type == 4 || m_type == 5 || m_type == 3 || m_type == 6)
	{
		m_rotation += m_rotationincrement;
		if(m_rotation > 30.0f)
		{
			m_rotationincrement *= -1;
			m_rotation = 30.0f;
		}
		else if(m_rotation < -30.0f)
		{
			m_rotationincrement *= -1;
			m_rotation = -30.0f;
		}
	}
}

// Cuboid c(0, 0, 0, 5, 5, 5);
vector<Cuboid *> field;
vector<Cuboid *> water;
Player *playerHead = NULL, *playerBody, *playerLeftLeg, *playerRightLeg, *playerLeftHand, *playerRightHand;

void initPlayer()
{
	if(playerHead != NULL)
	{
		delete playerHead, playerBody, playerLeftHand, playerLeftLeg, playerRightHand, playerRightLeg;
	}
	playerHead = new Player(0, 3, 0, 0.56, 0.56, 0.56);
	playerHead->createPlayer(texturePlayer, 1);
	playerBody = new Player(0.14, 2.44, 0, 1.12, 0.28, 0.56);
	playerBody->createPlayer(texturePlayer, 2);
	playerRightLeg = new Player(0.14, 1.42, 0, 1.22, 0.28, 0.28);
	playerRightLeg->createPlayer(texturePlayer, 6);
	playerLeftLeg = new Player(0.14, 1.42, -0.28, 1.22, 0.28, 0.28);
	playerLeftLeg->createPlayer(texturePlayer, 3);
	playerLeftHand = new Player(0.14, 2.44, 0.28, 1.12, 0.28, 0.28);
	playerLeftHand->createPlayer(texturePlayer, 4);
	playerRightHand = new Player(0.14, 2.44, -0.56, 1.12, 0.28, 0.28);
	playerRightHand->createPlayer(texturePlayer, 5);
}

void initWorld(GLuint TextureIDGrass, GLuint TextureIDWater, GLuint TextureIDLava, GLuint TextureIDDirt)
{
	findLavaBlocks();
	for(int i = 0; i < 50; i++)
	{
		for(int j = 0; j < 50; j++)
		{
			if(i >= 11 && i <= 40 && j >= 11 && j <= 40)
				continue;
			Cuboid *temp = new Cuboid(40-i, -1, 40-j, 1, 1, 1);
			temp->createCuboid(TextureIDWater, 2);
			water.push_back(temp);
		}
	}

	int k = 0, l = 1;

	for(int i = 0; i < 30; i++)
	{
		for(int j = 0; j < 30; j++)
		{
			Cuboid *temp;
			if(i * 30 + j == pits[k])
			{
				temp = new Cuboid(i, 0, j, 1, 1, 1);
				temp->createCuboid(TextureIDLava, 4);
				k += 2;
			}
			else if(i * 30 + j == pits[l])
			{
				temp = new Cuboid(i, -1, j, 1, 1, 1);
				temp->createCuboid(TextureIDGrass, 1);
				l += 2;
			}
			else
			{
				temp = new Cuboid(i, 0, j, 1, 1, 1);
				temp->createCuboid(TextureIDGrass, 1);
			}
			field.push_back(temp);

		}
	}

	initPlayer();
}

void drawWorld()
{
	for(vector<Cuboid *>::iterator it = water.begin(); it != water.end(); ++it)
	{
		(*it)->draw();
	}
	for(vector<Cuboid *>::iterator it = field.begin(); it != field.end(); ++it)
	{
		(*it)->draw();
	}
	playerHead->draw();
	playerBody->draw();
	playerLeftLeg->draw();
	playerRightLeg->draw();
	playerRightHand->draw();
	playerLeftHand->draw();
}

void moveForward()
{
	playerHead->moveForward();
	playerBody->moveForward();
	playerLeftLeg->moveForward();
	playerRightLeg->moveForward();
	playerRightHand->moveForward();
	playerLeftHand->moveForward();

}

void moveBackward()
{
	playerHead->moveBackward();
	playerBody->moveBackward();
	playerRightLeg->moveBackward();
	playerLeftLeg->moveBackward();
	playerRightHand->moveBackward();
	playerLeftHand->moveBackward();

}

void moveLeft()
{
	playerHead->moveLeft();
	playerBody->moveLeft();
	playerLeftLeg->moveLeft();
	playerRightLeg->moveLeft();
	playerRightHand->moveLeft();
	playerLeftHand->moveLeft();

}

void moveRight()
{
	playerHead->moveRight();
	playerBody->moveRight();
	playerLeftLeg->moveRight();
	playerRightLeg->moveRight();
	playerRightHand->moveRight();
	playerLeftHand->moveRight();

}

void cameraChange()
{
	if(defaultCam == 1)
	{
		// Eye - Location of camera. Don't change unless you are sure!!
		glm::vec3 eye ( playerHead->getX() + 5*cos(camera_rotation_angle*M_PI/180.0f), playerHead->getY() + 2, playerHead->getZ() + 5*sin(camera_rotation_angle*M_PI/180.0f) );
		// Target - Where is the camera looking at.  Don't change unless you are sure!!
		glm::vec3 target (playerHead->getX(), playerHead->getY(), playerHead->getZ());

		// Up - Up vector defines tilt of camera.  Don't change unless you are sure!!
		glm::vec3 up (0, 1, 0);

		// Compute Camera matrix (view)
		Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
		camera_rotation_angle -= 0.3; // Simulating camera rotation
	}
	else if(defaultCam == 2)
	{
		// Eye - Location of camera. Don't change unless you are sure!!
		glm::vec3 eye ( -1.5 + playerHead->getX(), 2 + playerHead->getY(), playerHead->getZ() - 0.28);
		// Target - Where is the camera looking at.  Don't change unless you are sure!!
		glm::vec3 target (playerHead->getX(), playerHead->getY(), playerHead->getZ()-0.28);

		// Up - Up vector defines tilt of camera.  Don't change unless you are sure!!
		glm::vec3 up (0, 0.1, 0);

		// Compute Camera matrix (view)
		Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
		camera_rotation_angle -= 0.1; // Simulating camera rotation
	}

	else if(defaultCam == 3)
	{
		glm::vec3 eye ( 14, 15, 15);
		glm::vec3 target (15, 0, 15);
		glm::vec3 up (0, 0.1, 0);
		Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
	}

	else if(defaultCam == 4)
	{
		glm::vec3 eye ( 0, 15, 15);
		glm::vec3 target (15, 0, 15);
		glm::vec3 up (0, 0.1, 0);
		Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
	}

}

/* Executed when a regular key is pressed/released/held-down */
/* Prefered for Keyboard events */
void keyboard (GLFWwindow* window, int key, int scancode, int action, int mods)
{
     // Function is called first on GLFW_PRESS.

    if (action == GLFW_RELEASE) {
        switch (key) {
            case GLFW_KEY_C:
                rectangle_rot_status = !rectangle_rot_status;
                break;
            case GLFW_KEY_P:
                triangle_rot_status = !triangle_rot_status;
                break;
            case GLFW_KEY_X:
                // do something ..
                break;
            default:
                break;
        }
    }
    else if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                quit(window);
                break;
            case GLFW_KEY_1:
            	defaultCam = 1;
            	break;
            case GLFW_KEY_2:
            	defaultCam = 2;
            	break;
            case GLFW_KEY_3:
            	defaultCam = 3;
            	break;
            case GLFW_KEY_4:
            	defaultCam = 4;
            	break;	
            default:
                break;
        }
    }
    else if (action == GLFW_REPEAT)
    {
    	switch(key)
    	{

    	}
    }
}

/* Executed for character input (like in text boxes) */
void keyboardChar (GLFWwindow* window, unsigned int key)
{
	switch (key) {
		case 'Q':
		case 'q':
            quit(window);
            break;
		default:
			break;
	}
}

/* Executed when a mouse button is pressed/released */
void mouseButton (GLFWwindow* window, int button, int action, int mods)
{
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            if (action == GLFW_RELEASE)
                triangle_rot_dir *= -1;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            if (action == GLFW_RELEASE) {
                rectangle_rot_dir *= -1;
            }
            break;
        default:
            break;
    }
}

GLFWwindow* window; // window desciptor/handle


void PollKeys()
{
	if(glfwGetKey(window, GLFW_KEY_UP))
		moveForward();
	if(glfwGetKey(window, GLFW_KEY_DOWN))
		moveBackward();
	if(glfwGetKey(window, GLFW_KEY_LEFT))
		moveLeft();
	if(glfwGetKey(window, GLFW_KEY_RIGHT))
		moveRight();
}

/* Initialise glfw window, I/O callbacks and the renderer to use */
/* Nothing to Edit here */
GLFWwindow* initGLFW (int width, int height)
{

    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Minecraft 0.02", NULL, NULL);

    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSwapInterval( 1 );

    /* --- register callbacks with GLFW --- */

    /* Register function to handle window resizes */
    /* With Retina display on Mac OS X GLFW's FramebufferSize
     is different from WindowSize */
    glfwSetFramebufferSizeCallback(window, reshapeWindow);
    glfwSetWindowSizeCallback(window, reshapeWindow);

    /* Register function to handle window close */
    glfwSetWindowCloseCallback(window, quit);

    /* Register function to handle keyboard input */
    glfwSetKeyCallback(window, keyboard);      // general keyboard input
    glfwSetCharCallback(window, keyboardChar);  // simpler specific character handling

    /* Register function to handle mouse click */
    glfwSetMouseButtonCallback(window, mouseButton);  // mouse button clicks

    return window;
}

/* Initialize the OpenGL rendering properties */
/* Add all the models to be created here */
void initGL (GLFWwindow* window, int width, int height)
{
	// Load Textures
	// Enable Texture0 as current texture memory
	glActiveTexture(GL_TEXTURE0);
    /* Objects should be created before any other gl function and shaders */
	// Create the models
	

	// load an image file directly as a new OpenGL texture
	// GLuint texID = SOIL_load_OGL_texture ("beach.png", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_TEXTURE_REPEATS); // Buggy for OpenGL3
	GLuint textureGrass = createTexture("grass.png");
	// check for an error during the load process
	if(textureGrass == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	GLuint textureLava = createTexture("lava2.png");
	// check for an error during the load process
	if(textureLava == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	GLuint textureWater = createTexture("water2.png");
	// check for an error during the load process
	if(textureWater == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	texturePlayer = createTexture("skin2.png");
	// check for an error during the load process
	if(texturePlayer == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	GLuint textureDirt = createTexture("dirt2.png");
	// check for an error during the load process
	if(textureDirt == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	// Create and compile our GLSL program from the texture shaders
	textureProgramID = LoadShaders( "TextureRender.vert", "TextureRender.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.TexMatrixID = glGetUniformLocation(textureProgramID, "MVP");

	/* Objects should be created before any other gl function and shaders */
	// Create the models
	// c.createCuboid(textureGrass, 1);
	initWorld(textureGrass, textureWater, textureLava, textureDirt);
	
	// Create and compile our GLSL program from the shaders
	programID = LoadShaders( "Sample_GL.vert", "Sample_GL.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.MatrixID = glGetUniformLocation(programID, "MVP");

	
	reshapeWindow (window, width, height);

    // Background color of the scene
	glClearColor (0.3f, 0.3f, 0.3f, 0.0f); // R, G, B, A
	glClearDepth (1.0f);

	glEnable (GL_DEPTH_TEST);
	//glDepthFunc (GL_LEQUAL);
	glDepthFunc(GL_LESS);

    cout << "VENDOR: " << glGetString(GL_VENDOR) << endl;
    cout << "RENDERER: " << glGetString(GL_RENDERER) << endl;
    cout << "VERSION: " << glGetString(GL_VERSION) << endl;
    cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

void draw ()
{
	// clear the color and depth in the frame buffer
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// use the loaded shader program
	// Don't change unless you know what you are doing
	glUseProgram(textureProgramID);

	cameraChange();

	// Compute ViewProject matrix as view/camera might not be changed for this frame (basic scenario)
	//  Don't change unless you are sure!!
	glm::mat4 VP = Matrices.projection * Matrices.view;

	// Send our transformation to the currently bound shader, in the "MVP" uniform
	// For each model you render, since the MVP will be different (at least the M part)
	//  Don't change unless you are sure!!
	glm::mat4 MVP;	// MVP = Projection * View * Model


	// Load identity to model matrix
	Matrices.model = glm::mat4(1.0f);
	drawWorld();

}

void lavacheck()
{
	int k = 0;
	// int cx = floor(playerBody->getX()), cz = floor(playerBody->getZ() + 0.56);
	float cx = playerBody->getX(), cz = playerBody->getZ() + 0.56;
	float cx1 = playerBody->getX(), cz1 = playerBody->getZ() + 0.94;
	int cxx = floor(cx), czz = floor(cz);
	int cxx1 = floor(cx1), czz1 = floor(cz1);
	int number = (cxx * 30) + czz;
	int number1 = (cxx1 * 30) + czz1;
	if(number >= 0 && number < 900)
	{
		if(field[number]->getBlockType() == 4)
		{
			initPlayer();
			return;
		}
	}
	if(number1 >= 0 && number1 < 900)
	{
		if	(field[number1]->getBlockType() == 4)
		{
			initPlayer();
			return;
		}
	}
}

int main (int argc, char** argv)
{
	int width = 1300;
	int height = 700;
	srand(time(0));

    GLFWwindow* window = initGLFW(width, height);

	initGL (window, width, height);

    double last_update_time = glfwGetTime(), current_time;

    /* Draw in loop */
    while (!glfwWindowShouldClose(window)) {

        // OpenGL Draw commands
        draw();
        // cout<<floor(playerHead->getX())<<endl;
        // c.draw();
        lavacheck();

        // Swap Frame Buffer in double buffering
        glfwSwapBuffers(window);

        PollKeys();

        // Poll for Keyboard and mouse events
        glfwPollEvents();

        // Control based on time (Time based transformation like 5 degrees rotation every 0.5s)
        current_time = glfwGetTime(); // Time in seconds
        if ((current_time - last_update_time) >= 0.5) { // atleast 0.5s elapsed since last frame
            // do something every 0.5 seconds ..

            last_update_time = current_time;
        }
        // for(int j = 0; j<4000000; j++);
    }

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
