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
#include <SFML/Audio.hpp>

#define YELLOW 1.0f, 1.0f, 0.0f

#define move_threshhold 1.2


using namespace std;

int defaultCam = 2, pan = 0;
int pits[895];
int lives = 5, score = 0, level = 1, oldlevel = 1, pitcount = 100, oldscore = 0;

char scoreboard[100];

GLuint texturePlayer, textureGrass, textureWater, textureLava, textureDirt;
sf::Music grassSound;
sf::Music levelupSound;

float heli_x, heli_y, heli_z, old_helix, old_heliy, old_heliz, helirotation = 0;
double panoldx, panoldy, panx, pany;

float eyex, eyey, eyez, targetx, targety, targetz, upx, upy, upz, fontx, fonty, fontz, fontrotation;

GLFWwindow* window; // window desciptor/handle


struct VAO 
{
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

struct GLMatrices 
{
	glm::mat4 projection;
	glm::mat4 model;
	glm::mat4 view;
	GLuint MatrixID; // For use with normal shader
	GLuint TexMatrixID; // For use with texture shader
} Matrices;

struct FTGLFont 
{
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

	qsort(pits, pitcount, sizeof(int), compare);

	// for(int i = 0; i < 100; i++)
	// 	cout<<pits[i]<<endl;

}

void initPlayer();

void resetWorld()
{
	lives = 5; score = 0; level = 1; oldlevel = 0; pitcount = 100; oldscore = 0;
	defaultCam = 2, pan = 0;
	initPlayer();
}


GLuint programID, textureProgramID, fontProgramID;

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
		float m_height, m_width, m_length, m_upspeed;
		float m_maxheight, m_minheight;
		GLfloat m_vertex_buffer_data[12*3*3];
		GLfloat m_color_buffer_data[12*3*3];
		GLfloat m_texture_buffer_data[12*3*2];
		VAO *m_cuboid;
		GLuint m_textureID;
		int m_blocktype;

	public:
		Cuboid(float x, float y, float z, float length, float width, float height, float maxheight, float minheight, float upspeed);
		Cuboid(Cuboid &c);
		void createCuboid(GLuint, int);
		void draw();
		void grassBlock();
		void waterBlock();
		float getX() { return m_x; }
		float getY() { return m_y; }
		float getZ() { return m_z; }
		float getHeight() { return m_height; }
		float getWidth() { return m_width; }
		float getLength() { return m_length; }
		int getBlockType() { return m_blocktype; }
		void setY(float y) { m_y = y; }
		void oscillateBlock();
};

Cuboid::Cuboid(float x, float y, float z, float length, float width, float height, float maxheight = 10.0f, float minheight = -9.0f, float upspeed = 0.1f)
{
	m_x = x;
	m_y = y;
	m_z = z;
	m_length = length;
	m_width = width;
	m_height = height;
	m_rotation = 0;
	m_cuboid = NULL;
	m_maxheight = maxheight;
	m_minheight = minheight;
	m_upspeed = upspeed;
	if(rand()%2 == 0)
		m_upspeed *= -1;
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
	m_maxheight = c.m_maxheight;
	m_minheight = c.m_minheight;
	m_upspeed = c.m_upspeed;
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

/* Render the scene with openGL */
/* Edit this function according to your assignment */
void Cuboid::draw ()
{
	glm::mat4 MVP;	// MVP = Projection * View * Model

	Matrices.model = glm::mat4(1.0f);
	glm::mat4 translateCuboid = glm::translate (glm::vec3(m_x, m_y, m_z));  
	glm::mat4 rotateCuboid = glm::rotate((float)(m_rotation*M_PI/180.0f), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)
	Matrices.model *= (translateCuboid * rotateCuboid);
	MVP = Matrices.projection * Matrices.view* Matrices.model;

	glUniformMatrix4fv(Matrices.TexMatrixID, 1, GL_FALSE, &MVP[0][0]);

	// Set the texture sampler to access Texture0 memory
	glUniform1i(glGetUniformLocation(textureProgramID, "texSampler"), 0);

	// draw3DObject draws the VAO given to it using current MVP matrix
	draw3DTexturedObject(m_cuboid);
}

void Cuboid::oscillateBlock()
{
	m_y += m_upspeed;
	if(m_y >= m_maxheight || m_y <= m_minheight)
		m_upspeed = -m_upspeed;
}

class Player : public Cuboid
{
	private:
		int m_type;
		float m_totalmove, m_rotationincrement;
		float m_speed, m_speed2;

	public:
		Player(float x, float y, float z, float length, float width, float height);
		Player(Player &p);
		void createPlayer(GLuint texturePlayer, int part);
		void moveForward();
		void moveBackward();
		void moveLeft();
		void moveRight();
		void moveFR();
		void moveFL();
		void moveDR();
		void moveDL();
		void moveUp();
		void moveDown();
		void moveAnimation();
		void playerHeadBlock();
		void playerBodyBlock();
		void playerLegBlock();
		void playerRightHandBlock();
		void playerLeftHandBlock();
		void moveFast() { if(m_speed <= 0.24) m_speed += 0.02; }
		void moveSlow() { if(m_speed >= 0.10) m_speed -= 0.02; }

};

Player::Player(float x, float y, float z, float length, float width, float height): Cuboid(x, y, z, length, width, height)
{
	m_type = 0;
	m_speed = 0.1;
	m_speed2 = 0.1;
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

void Player::moveAnimation()
{
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

void Player::moveForward()
{
	m_x += m_speed;
	moveAnimation();
}

void Player::moveBackward()
{
	m_x -= m_speed;
	moveAnimation();
}

void Player::moveLeft()
{
	m_z -= m_speed;
	moveAnimation();
}

void Player::moveRight()
{
	m_z += m_speed;
	moveAnimation();
}

void Player::moveFR()
{
	m_x += sqrt(m_speed * m_speed / 2.0f);
	m_z += sqrt(m_speed * m_speed / 2.0f);
	moveAnimation();
}

void Player::moveFL()
{
	m_x += sqrt(m_speed * m_speed / 2.0f);
	m_z -= sqrt(m_speed * m_speed / 2.0f);
	moveAnimation();
}

void Player::moveDR()
{
	m_x -= sqrt(m_speed * m_speed / 2.0f);
	m_z += sqrt(m_speed * m_speed / 2.0f);
	moveAnimation();
}
void Player::moveDL()
{
	m_x -= sqrt(m_speed * m_speed / 2.0f);
	m_z -= sqrt(m_speed * m_speed / 2.0f);
	moveAnimation();
}

void Player::moveUp()
{
	m_y += m_speed2;
}

void Player::moveDown()
{
	m_y -= m_speed2;
}

// Cuboid c(0, 0, 0, 5, 5, 5);
vector<Cuboid *> field, pillars;
vector<Cuboid *> water;
Player *playerHead = NULL, *playerBody, *playerLeftLeg, *playerRightLeg, *playerLeftHand, *playerRightHand;
int air = 0, direction;
float highthreshold = 1.5, jumpheight, jumpthreshold = 2.5;


void refreshWorld();

void initPlayer()
{
	if(level != oldlevel)
	{
		oldlevel = level;
		oldscore = score;
		refreshWorld();
	}
	lives--;
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

void refreshWorld()
{
	findLavaBlocks();
	for(int i = 0; i < 30; i++)
	{
		for(int j = 0; j < 30; j++)
		{
			if(field[i*30 + j]->getBlockType() == 4 || field[i*30 + j]->getBlockType() == 5)
			{
				Cuboid *temp = field[i*30 + j];
				delete temp;
				temp = new Cuboid(i, 0, j, 1, 1, 1);
				temp->createCuboid(textureGrass, 1);
				field[i*30 + j] = temp;
			}
		}
	}

	while(!pillars.empty())
		pillars.pop_back();

	int k = 0, l = 1;

	for(int i = 0; i < 30; i++)
	{
		for(int j = 0; j < 30; j++)
		{
			Cuboid *temp;
			if(i * 30 + j == pits[k])
			{
				temp = field[i*30 + j];
				delete temp;
				temp = new Cuboid(i, 0, j, 1, 1, 1);
				temp->createCuboid(textureLava, 4);
				k += 2;
				field[i*30 + j] = temp;

			}
			else if(i * 30 + j == pits[l])
			{
				temp = field[i*30 + j];
				delete temp;
				if(rand() % 2 == 0)
				{
					temp = new Cuboid(i, -1, j, 1, 1, 1);
					temp->createCuboid(textureGrass, 5);
					pillars.push_back(temp);
				}
				else
				{
					temp = new Cuboid(i, +1, j, 1, 1, 1);
					temp->createCuboid(textureGrass, 5);
				}
				field[i*30 + j] = temp;
				l+=2;
			}
		}
	}

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
				if(rand() % 2 == 0)
				{
					temp = new Cuboid(i, -1, j, 1, 1, 1);
					temp->createCuboid(TextureIDGrass, 5);
					pillars.push_back(temp);
				}
				else
				{
					temp = new Cuboid(i, +1, j, 1, 1, 1);
					temp->createCuboid(TextureIDGrass, 5);
				}
				l += 2;
			}
			else if(i * 30 + j == 899)
			{
				temp = new Cuboid(i, 0, j, 1, 1, 1);
				temp->createCuboid(TextureIDDirt, 1);
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
	if(grassSound.getStatus() == sf::SoundSource::Stopped || grassSound.getStatus() == sf::SoundSource::Paused)
		grassSound.play();
	playerHead->moveForward();
	playerBody->moveForward();
	playerLeftLeg->moveForward();
	playerRightLeg->moveForward();
	playerRightHand->moveForward();
	playerLeftHand->moveForward();

}

void moveBackward()
{
	if(grassSound.getStatus() == sf::SoundSource::Stopped || grassSound.getStatus() == sf::SoundSource::Paused)
		grassSound.play();
	playerHead->moveBackward();
	playerBody->moveBackward();
	playerRightLeg->moveBackward();
	playerLeftLeg->moveBackward();
	playerRightHand->moveBackward();
	playerLeftHand->moveBackward();

}

void moveLeft()
{
	if(grassSound.getStatus() == sf::SoundSource::Stopped || grassSound.getStatus() == sf::SoundSource::Paused)
		grassSound.play();
	playerHead->moveLeft();
	playerBody->moveLeft();
	playerLeftLeg->moveLeft();
	playerRightLeg->moveLeft();
	playerRightHand->moveLeft();
	playerLeftHand->moveLeft();

}

void moveRight()
{
	if(grassSound.getStatus() == sf::SoundSource::Stopped || grassSound.getStatus() == sf::SoundSource::Paused)
		grassSound.play();
	playerHead->moveRight();
	playerBody->moveRight();
	playerLeftLeg->moveRight();
	playerRightLeg->moveRight();
	playerRightHand->moveRight();
	playerLeftHand->moveRight();

}

void moveFR()
{
	if(grassSound.getStatus() == sf::SoundSource::Stopped || grassSound.getStatus() == sf::SoundSource::Paused)
		grassSound.play();
	playerHead->moveFR();
	playerBody->moveFR();
	playerLeftLeg->moveFR();
	playerRightLeg->moveFR();
	playerRightHand->moveFR();
	playerLeftHand->moveFR();
}

void moveFL()
{
	if(grassSound.getStatus() == sf::SoundSource::Stopped || grassSound.getStatus() == sf::SoundSource::Paused)
		grassSound.play();
	playerHead->moveFL();
	playerBody->moveFL();
	playerLeftLeg->moveFL();
	playerRightLeg->moveFL();
	playerRightHand->moveFL();
	playerLeftHand->moveFL();
}

void moveDR()
{
	if(grassSound.getStatus() == sf::SoundSource::Stopped || grassSound.getStatus() == sf::SoundSource::Paused)
		grassSound.play();
	playerHead->moveDR();
	playerBody->moveDR();
	playerLeftLeg->moveDR();
	playerRightLeg->moveDR();
	playerRightHand->moveDR();
	playerLeftHand->moveDR();
}

void moveDL()
{
	if(grassSound.getStatus() == sf::SoundSource::Stopped || grassSound.getStatus() == sf::SoundSource::Paused)
		grassSound.play();
	playerHead->moveDL();
	playerBody->moveDL();
	playerLeftLeg->moveDL();
	playerRightLeg->moveDL();
	playerRightHand->moveDL();
	playerLeftHand->moveDL();
}

void moveUp()
{
	playerHead->moveUp();
	playerBody->moveUp();
	playerLeftLeg->moveUp();
	playerRightLeg->moveUp();
	playerRightHand->moveUp();
	playerLeftHand->moveUp();
}

void moveDown()
{
	playerHead->moveDown();
	playerBody->moveDown();
	playerLeftLeg->moveDown();
	playerRightLeg->moveDown();
	playerRightHand->moveDown();
	playerLeftHand->moveDown();
}

void moveSlow()
{
	playerHead->moveSlow();
	playerBody->moveSlow();
	playerLeftLeg->moveSlow();
	playerRightLeg->moveSlow();
	playerRightHand->moveSlow();
	playerLeftHand->moveSlow();
}

void moveFast()
{
	playerHead->moveFast();
	playerBody->moveFast();
	playerLeftLeg->moveFast();
	playerRightLeg->moveFast();
	playerRightHand->moveFast();
	playerLeftHand->moveFast();
}

int getRightCurrentBlock(int op = 1)
{
	float cx = playerBody->getX(), cz = playerBody->getZ() + 0.56;
	if(op == 2)
	{
		cz += 0.28;
		cx += 0.14;
	}
	else if(op == 3)
	{
		cz -= 0.56;
	}
	int cxx = floor(cx), czz = floor(cz);
	int number = (cxx * 30) + czz;
	return number;
}

int getCurrentBlock(int op = 1)
{
	float cx = playerBody->getX(), cz = playerBody->getZ();
	if(op == 2)
	{
		cz -= 0.28;
		cx -= 0.14;
	}
	else if(op == 3)
	{
		cz -= 0.56;
	}

	if(cz > 29 || cz < -1)
		return -1;

	int cxx = floor(cx), czz = ceil(cz);
	int number = (cxx * 30) + czz;
	return number;
}

void jump()
{
	if(air == 0)
	{
		jumpheight = playerLeftLeg->getY();
		air = 1;
		direction = 1;
	}

	if(direction == 1)
		moveUp();
	else
		moveDown();
	int k = 0;
	
	float cy = playerLeftLeg->getY();
	int number = getCurrentBlock();
	int number1 = getCurrentBlock(3);

	float ground = water[0]->getY(), ground1 = water[0]->getY();
	if(number >= 0 && number < 900)
		ground = field[number]->getY();
	if(number1 >= 0 && number1 < 900)
		ground1 = field[number1]->getY();

	float max = (ground > ground1) ? ground : ground1;

	if(direction == 1 && cy - jumpheight >= highthreshold )
	{
		direction = -1;
	}

	if(number >= 0 && number < 900)
	{
		if(cy < max + playerLeftLeg->getLength())
		{
			moveUp();
			air = 0;
			if(jumpheight - cy >= jumpthreshold)
				initPlayer();
		}
	}
	else
	{
		if(playerBody->getY() < water[0]->getY())
		{
			air = 0;
			initPlayer();
		}
	}
}

// void stayWithGround();

void movePillars()
{
	for(vector<Cuboid *>::iterator it = pillars.begin(); it != pillars.end(); ++it)
		(*it)->oscillateBlock();

}	

void changePlayerY(float y)
{
	playerLeftLeg->setY(y + playerLeftLeg->getLength());
	playerRightLeg->setY(y + playerLeftLeg->getLength());
	playerBody->setY(playerLeftLeg->getY() + playerBody->getLength());
	playerRightHand->setY(playerBody->getY());
	playerLeftHand->setY(playerBody->getY());
	playerHead->setY(playerBody->getY() + playerHead->getLength());
}

void stayWithGround()
{
	if(air == 0)
	{
		int number = getCurrentBlock();
		int number1 = getCurrentBlock(3);
		float ground = water[0]->getY(), ground1 = water[0]->getY();
		if(number >= 0 && number < 900)
			ground = field[number]->getY();
		if(number1 >= 0 && number1 < 900)
			ground1 = field[number1]->getY();

		float max = (ground > ground1) ? ground : ground1;
		if(max <= playerLeftLeg->getY())
			changePlayerY(max);
	}
}

void prepareHeliCam(GLFWwindow *window)
{
	glfwSetCursorPos(window, 650, 350);
	heli_x = old_helix = 16;
	heli_y = old_heliy = 15;
	heli_z = old_heliz = 15;
	helirotation = 0;
}

void heliCamMove(GLFWwindow *window)
{
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	
	float xratio = 26.0 / 15.0;
	float zratio = 52.0 / 15.0;
	float xpixelRatio = (xratio * heli_y) / 700.0;
	float zpixelRatio = (zratio * heli_y) / 1300.0;

	float xmove = 350 - ypos;
	float totalxmove = xmove * xpixelRatio;

	heli_x = old_helix + totalxmove;

	float zmove = xpos - 650;
	float totalzmove = zmove * zpixelRatio;

	heli_z = old_heliz + totalzmove;

}

void heliCamPan(GLFWwindow *window)
{
	glfwGetCursorPos(window, &panx, &pany);

	float yoffset = panoldy - pany;
	helirotation = (90.0/350.0) * yoffset;
}

void cameraChange()
{
	if(defaultCam == 1)
	{
		eyex = playerHead->getX() + 5*cos(camera_rotation_angle*M_PI/180.0f);
		eyey = playerHead->getY() + 2;
		eyez = playerHead->getZ() + 5*sin(camera_rotation_angle*M_PI/180.0f);

		targetx = playerHead->getX();
		targety = playerHead->getY();
		targetz = playerHead->getZ();

		upx = 0;
		upy = 1;
		upz = 0;

		fontx = eyex + 2;
		fonty = eyey + 0.2;
		fontz = eyez;
		fontrotation = -90.0f;

		camera_rotation_angle -= 0.3; // Simulating camera rotation
	}
	else if(defaultCam == 2)
	{
		eyex = -1.75 + playerHead->getX();
		eyey = 2 + playerHead->getY();
		eyez = playerHead->getZ() - 0.28;

		targetx = playerHead->getX();
		targety = playerHead->getY();
		targetz = playerHead->getZ() - 0.28;

		upx = 0;
		upy = 1;
		upz = 0;

		fontx = eyex + 2;
		fonty = eyey + 0.2;
		fontz = eyez - 1;
		fontrotation = -90.0f;

	}

	else if(defaultCam == 3)
	{
		eyex = 15;
		eyey = 15;
		eyez = 15;

		targetx = 15;
		targety = -515;
		targetz = 15;

		upx = 1;
		upy = 0;
		upz = 0;

		fontx = eyex + 1.2;
		fonty = eyey - 1;
		fontz = eyez - 1;
		fontrotation = -90.0f;
	}

	else if(defaultCam == 4)
	{
		eyex = -5;
		eyey = 15;
		eyez = 15;

		targetx = 15;
		targety = 0;
		targetz = 15;

		upx = 0;
		upy = 1;
		upz = 0;

		fontx = eyex + 2;
		fonty = eyey + 0.6;
		fontz = eyez - 1;
		fontrotation = -90.0f;

	}

	else if(defaultCam == 5)
	{
		eyex = playerHead->getX() + playerHead->getLength();
		eyey = playerHead->getY();
		eyez = playerHead->getZ() - 0.28;

		targetx = playerHead->getX() + 500;
		targety = playerHead->getY();
		targetz = playerHead->getZ();

		upx = 0;
		upy = 1;
		upz = 0;

		fontx = eyex + 2;
		fonty = eyey + 2.7;
		fontz = eyez - 1;
		fontrotation = -90.0f;

	}

	else if(defaultCam == 6)
	{
		if(pan == 1)
        	heliCamPan(window);

		heliCamMove(window);

		eyex = heli_x;
		eyey = heli_y;
		eyez = heli_z;

		targety = 0;
		targetz = heli_z;

		if(helirotation >= 0)
		{
			targetx = (2 * heli_x) - (heli_x * cos(helirotation * M_PI / 180.0f));
		}
		else
		{
			targetx = heli_x * cos(helirotation * M_PI / 180.0f);
		}

		upx = 1;
		upy = 0;
		upz = 0;

		fontx = eyex + 1.2;
		fonty = eyey - 1;
		fontz = eyez - 1;
		fontrotation = -90.0f;
	}

	glm::vec3 eye(eyex, eyey, eyez);
	glm::vec3 target(targetx, targety, targetz);
	glm::vec3 up(upx, upy, upz);
	
	Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D

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
            case GLFW_KEY_UP:
            case GLFW_KEY_DOWN:
            case GLFW_KEY_LEFT:
            case GLFW_KEY_RIGHT:
            	grassSound.pause();
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
            case GLFW_KEY_5:
            	defaultCam = 5;
            	break;	
            case GLFW_KEY_6:
            	prepareHeliCam(window);
            	defaultCam = 6;
            	break;	
            case GLFW_KEY_SPACE:
            	if(air == 0)
            		jump();
            case GLFW_KEY_F:
            	moveFast();
            	break;
            case GLFW_KEY_S:
            	moveSlow();
            	break;
            case GLFW_KEY_R:
            	resetWorld();
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
            // if (action == GLFW_RELEASE)
            //     triangle_rot_dir *= -1;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            if (action == GLFW_PRESS) 
            {
            	pan = 1;
            	glfwGetCursorPos(window, &panoldx, &panoldy);
            }
            else if(action == GLFW_RELEASE)
            {
            	pan = 0;
            	glfwSetCursorPos(window, 650, 350);
            }
            break;
        default:
            break;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	if(defaultCam != 6)
		return;
	if(yoffset > 0)
		heli_y -= 0.3;
	else
		heli_y += 0.3;
	old_helix = heli_x;
	old_heliz = heli_z;
	glfwSetCursorPos(window, 650, 350);

}


void PollKeys()
{
	if(glfwGetKey(window, GLFW_KEY_UP) && glfwGetKey(window, GLFW_KEY_LEFT))
	{
		int p1 = -1, p2 = -1;
		int number = getCurrentBlock();
		int number1;
		if(number >=0 && number < 900)
		{
			if(field[number]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				p1 = 1;
			}
			else
				p1 = 0;
		}

		number = getCurrentBlock() + 30;
		number1 = getCurrentBlock(3) + 30;
		if(number >=0 && number < 900)
		{
			if(field[number]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength() && field[number1]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				p2 = 1;
			}
			else
				p2 = 0;
		}
		if(p1 && !p2)
			moveLeft();
		else if(!p1 && p2)
			moveForward();
		else if((p1 == -1 && p2 == -1) || (p1 && p2))
			moveFL();
	}
	else if(glfwGetKey(window, GLFW_KEY_UP) && glfwGetKey(window, GLFW_KEY_RIGHT))
	{
		int p1 = -1, p2 = -1;
		int number = getCurrentBlock();
		int number1;
		if(number >=0 && number < 900)
		{
			if(field[number]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				p1 = 1;
			}
			else
				p1 = 0;
		}

		number = getCurrentBlock() + 30;
		number1 = getCurrentBlock(3) + 30;
		if(number >=0 && number < 900)
		{
			if(field[number]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength() && field[number1]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				p2 = 1;
			}
			else
				p2 = 0;
		}
		if(p1 && !p2)
			moveRight();
		else if(!p1 && p2)
			moveForward();
		else if((p1 == -1 && p2 == -1) || (p1 && p2))
			moveFR();
	}
	else if(glfwGetKey(window, GLFW_KEY_DOWN) && glfwGetKey(window, GLFW_KEY_LEFT))
	{
		int p1 = -1, p2 = -1;
		int number = getCurrentBlock(3);
		int number1;
		if(number >=0 && number < 900)
		{
			if(field[number]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength() && playerBody->getZ() >= -1.20)
			{
				p1 = 1;
			}
			else
				p1 = 0;
		}

		number = getCurrentBlock() - 30;
		number1 = getCurrentBlock(3) - 30;
		if(number >=0 && number < 900)
		{
			if(field[number]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength() && field[number1]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				p2 = 1;
			}
			else
				p2 = 0;
		}
		if(p1 && !p2)
			moveLeft();
		else if(!p1 && p2)
			moveBackward();
		else if((p1 == -1 && p2 == -1) || (p1 && p2))
			moveDL();
	}
	else if(glfwGetKey(window, GLFW_KEY_DOWN) && glfwGetKey(window, GLFW_KEY_RIGHT))
	{
		int p1 = -1, p2 = -1;
		int number = getCurrentBlock();
		int number1;
		if(number >=0 && number < 900)
		{
			if(field[number]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				p1 = 1;
			}
			else
				p1 = 0;
		}

		number = getCurrentBlock() - 30;
		number1 = getCurrentBlock(3) - 30;
		if(number >=0 && number < 900)
		{
			if(field[number]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength() && field[number1]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				p2 = 1;
			}
			else
				p2 = 0;
		}
		if(p1 && !p2)
			moveRight();
		else if(!p1 && p2)
			moveBackward();
		else if((p1 == -1 && p2 == -1) || (p1 && p2))
			moveDR();
	}
	else if(glfwGetKey(window, GLFW_KEY_UP))
	{
		int number = getCurrentBlock() + 30;
		int number1 = getCurrentBlock(3) + 30;

		if((number >=0 && number < 900 ) && (number1 >=0 && number1 < 900))
		{
			if(field[number]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength() && field[number1]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				moveForward();
			}
		}
		else
			moveForward();
	}
	else if(glfwGetKey(window, GLFW_KEY_DOWN))
	{
		int number = getCurrentBlock() - 30;
		int number1 = getCurrentBlock(3) - 30;
		if((number >=0 && number < 900 ) && (number1 >=0 && number1 < 900))
		{
			if(field[number]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength() && field[number1]->getY() <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				moveBackward();
			}
		}
		else
			moveBackward();
	}
	else if(glfwGetKey(window, GLFW_KEY_LEFT))
	{
		int number = getCurrentBlock(3);
		float ground;
		if(number % 30 == 0 || number < 0)
			ground = water[0]->getY();
		else
			ground = field[number]->getY();
		if(number >=0 && number < 900)
		{
			if(ground <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				moveLeft();
			}
		}
		else
		{
			moveLeft();
		}
	}
	else if(glfwGetKey(window, GLFW_KEY_RIGHT))
	{
		int number = getCurrentBlock();
		float ground;
		if(number % 30 == 0 || number < 0 || number > 899)
			ground = water[0]->getY();
		else
			ground = field[number]->getY();
		if(number >=0 && number < 900)
		{
			if(ground <= playerLeftLeg->getY() - playerLeftLeg->getLength())
			{
				moveRight();
			}
		}
		else
		{
			moveRight();
		}
	}

	int number = getCurrentBlock();
	int number1 = getCurrentBlock(3);
	int ground, ground1;
	if(number >=0 && number < 900)
		ground = field[number]->getY();
	else
	{
		ground = water[0]->getY();
	}
	if(number1 >=0 && number1 < 900)
		ground1 = field[number1]->getY();
	else
	{
		ground1 = water[0]->getY();
	}
	if(((playerLeftLeg->getY() - playerLeftLeg->getLength() - ground > 0.2 && playerLeftLeg->getY() - playerLeftLeg->getLength() - ground1 > 0.2) || (number < 0 && number1 < 0)) && air == 0)
	{
		jumpheight = playerLeftLeg->getY();
		air = 1;
		direction = -1;
	}
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
    glfwSetScrollCallback(window, scroll_callback);

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
	textureGrass = createTexture("grass.png");
	// check for an error during the load process
	if(textureGrass == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	textureLava = createTexture("lava.png");
	// check for an error during the load process
	if(textureLava == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	textureWater = createTexture("water.png");
	// check for an error during the load process
	if(textureWater == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	texturePlayer = createTexture("skin.png");
	// check for an error during the load process
	if(texturePlayer == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	textureDirt = createTexture("dirt.png");
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

	const char* fontfile = "arial.ttf";
	GL3Font.font = new FTExtrudeFont(fontfile); // 3D extrude style rendering

	if(GL3Font.font->Error())
	{
		cout << "Error: Could not load font `" << fontfile << "'" << endl;
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	fontProgramID = LoadShaders( "fontrender.vert", "fontrender.frag" );
	GLint fontVertexCoordAttrib, fontVertexNormalAttrib, fontVertexOffsetUniform;
	fontVertexCoordAttrib = glGetAttribLocation(fontProgramID, "vertexPosition");
	fontVertexNormalAttrib = glGetAttribLocation(fontProgramID, "vertexNormal");
	fontVertexOffsetUniform = glGetUniformLocation(fontProgramID, "pen");
	GL3Font.fontMatrixID = glGetUniformLocation(fontProgramID, "MVP");
	GL3Font.fontColorID = glGetUniformLocation(fontProgramID, "fontColor");

	GL3Font.font->ShaderLocations(fontVertexCoordAttrib, fontVertexNormalAttrib, fontVertexOffsetUniform);
	GL3Font.font->FaceSize(1);
	GL3Font.font->Depth(0);
	GL3Font.font->Outset(0, 0);
	GL3Font.font->CharMap(ft_encoding_unicode);

	
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

void drawFont()
{
	glm::mat4 MVP;
	static int fontScale = 0;
  	glm::vec3 fontColor = getRGBfromHue (fontScale);
  	glUseProgram(fontProgramID);

	Matrices.model = glm::mat4(1.0f);
  	glm::mat4 translateText = glm::translate(glm::vec3(fontx, fonty, fontz)); 
  	glm::mat4 scaleText = glm::scale(glm::vec3(0.25, 0.25, 0.25));
  	glm::mat4 rotateText = glm::rotate((float)( fontrotation * M_PI / 180.0f ), glm::vec3(0, 1, 0)); 

	Matrices.model *= (translateText * scaleText * rotateText);
	if(defaultCam == 3 || defaultCam == 6)
	{
		glm::mat4 camerafix = glm::rotate((float)( -M_PI / 2.0f), glm::vec3(1, 0, 0));
		Matrices.model *= camerafix;
	}

	MVP = Matrices.projection * Matrices.view * Matrices.model;

	glUniformMatrix4fv(GL3Font.fontMatrixID, 1, GL_FALSE, &MVP[0][0]);
  	glUniform3fv(GL3Font.fontColorID, 1, &fontColor[0]);

  	sprintf(scoreboard, "Lives: %d  Level: %d   Score: %d", lives, level, score);

  	if(lives < 0)
  		sprintf(scoreboard, "Game Over. Level: %d Score %d Restart - R", level, score);

	GL3Font.font->Render(scoreboard);

	// font size and color changes
	fontScale = (fontScale + 1) % 360;

}

void lavacheck()
{
	int k = 0;
	int number = getCurrentBlock();
	int number1 = getCurrentBlock(3);
	if(number >= 0 && number < 900)
	{
		if(playerLeftLeg->getY() - playerLeftLeg->getLength() - field[number]->getY() <= 0.2 && field[number]->getBlockType() == 4)
		{
			initPlayer();
			return;
		}
	}
	if(number1 >= 0 && number1 < 900)
	{
		if	(playerLeftLeg->getY() - playerLeftLeg->getLength() - field[number1]->getY() <= 0.2 && field[number1]->getBlockType() == 4)
		{
			initPlayer();
			return;
		}
	}
}

void initSound()
{
	grassSound.openFromFile("grass.ogg");
	grassSound.setLoop(true);
	levelupSound.openFromFile("levelup.ogg");
	levelupSound.setLoop(false);
}

void fallcheck()
{
	int number = getCurrentBlock();
	int number1 = getCurrentBlock(3);
	if(number >= 0 && number < 900)
	{
		if(field[number]->getBlockType() == 5)
		{
			if(playerLeftLeg->getY() - playerLeftLeg->getLength() - field[number]->getY() <= 0.1)
			{
				jumpheight = playerHead->getY();
				return;
			}
		}
	}

	if(number1 >= 0 && number1 < 900)
	{
		if(field[number1]->getBlockType() == 5)
		{
			if(playerLeftLeg->getY() - playerLeftLeg->getLength() - field[number1]->getY() <= 0.1)
			{
				jumpheight = playerHead->getY();
				return;
			}
		}
	}
}

void goalCheck()
{
	int number = getCurrentBlock();
	int number1 = getCurrentBlock(3);
	if(number == 899 || number1 == 899)
	{
		level++;
		levelupSound.play();
		lives++;
		pitcount += 60;
		initPlayer();
	}
}

int main (int argc, char** argv)
{
	int width = 1300;
	int height = 700;
	srand(time(0));
	initSound();

    GLFWwindow* window = initGLFW(width, height);

	initGL (window, width, height);

    double last_update_time = glfwGetTime(), current_time;
    double last_score_time = glfwGetTime(), cur_score_time;

    /* Draw in loop */
    while (!glfwWindowShouldClose(window)) {

        // OpenGL Draw commands
        draw();
    	drawFont();
        lavacheck();

        // Swap Frame Buffer in double buffering
        glfwSwapBuffers(window);

        if(lives >= 0)
        	PollKeys();

        glfwPollEvents();

        current_time = glfwGetTime();

        if ((current_time - last_update_time) >= 0.01) 
        { 
            last_update_time = current_time;            
            movePillars();
            if(air != 0)
            	jump();
        }

        cur_score_time = glfwGetTime();

        if (cur_score_time - last_score_time >= 1 && lives >= 0) 
        {
        	last_score_time = cur_score_time;
        	score++;
        }
        stayWithGround();
        fallcheck();
        goalCheck();
    }

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
