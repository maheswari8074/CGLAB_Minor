// wireframe_3d_transform_fixed.cpp
// Single-file OpenGL (freeGLUT) example for Code::Blocks (MinGW).
// - Software DDA line algorithm (GL_POINTS).
// - 4x4 homogeneous matrices with operator* overloads.
// - Orthographic & Perspective projection (manual).
// - Interactive keyboard controls (see below).
//
// Linker flags (Windows MinGW + freeGLUT):
//   -lfreeglut -lopengl32 -lglu32

#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstring>
#include <algorithm>

const int WINDOW_W = 900;
const int WINDOW_H = 700;

struct Vec4 {
    float x, y, z, w;
};
inline Vec4 makeVec(float x, float y, float z){ return {x,y,z,1.0f}; }

struct Mat4 {
    float m[4][4];
    Mat4() { memset(m, 0, sizeof(m)); }
    static Mat4 identity() {
        Mat4 I; for (int i=0;i<4;i++) I.m[i][i]=1.0f; return I;
    }

    // Mat4 * Mat4
    Mat4 operator*(const Mat4& other) const {
        Mat4 r;
        for(int i=0;i<4;i++){
            for(int j=0;j<4;j++){
                float s = 0.0f;
                for(int k=0;k<4;k++) s += m[i][k] * other.m[k][j];
                r.m[i][j] = s;
            }
        }
        return r;
    }

    // apply matrix to Vec4: result = this * v
    Vec4 operator*(const Vec4& v) const {
        Vec4 r;
        r.x = m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z + m[0][3]*v.w;
        r.y = m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z + m[1][3]*v.w;
        r.z = m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z + m[2][3]*v.w;
        r.w = m[3][0]*v.x + m[3][1]*v.y + m[3][2]*v.z + m[3][3]*v.w;
        return r;
    }
};

// Basic transforms
Mat4 translate(float tx, float ty, float tz){
    Mat4 M = Mat4::identity();
    M.m[0][3] = tx; M.m[1][3] = ty; M.m[2][3] = tz;
    return M;
}
Mat4 scaleM(float sx, float sy, float sz){
    Mat4 M; memset(M.m,0,sizeof(M.m));
    M.m[0][0]=sx; M.m[1][1]=sy; M.m[2][2]=sz; M.m[3][3]=1;
    return M;
}
Mat4 rotateX(float deg){
    float r = deg * M_PI / 180.0f;
    Mat4 M = Mat4::identity();
    M.m[1][1] = cosf(r); M.m[1][2] = -sinf(r);
    M.m[2][1] = sinf(r); M.m[2][2] = cosf(r);
    return M;
}
Mat4 rotateY(float deg){
    float r = deg * M_PI / 180.0f;
    Mat4 M = Mat4::identity();
    M.m[0][0] = cosf(r); M.m[0][2] = sinf(r);
    M.m[2][0] = -sinf(r); M.m[2][2] = cosf(r);
    return M;
}
Mat4 rotateZ(float deg){
    float r = deg * M_PI / 180.0f;
    Mat4 M = Mat4::identity();
    M.m[0][0] = cosf(r); M.m[0][1] = -sinf(r);
    M.m[1][0] = sinf(r); M.m[1][1] = cosf(r);
    return M;
}

// Projection matrices
Mat4 makeOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp){
    Mat4 M; memset(M.m,0,sizeof(M.m));
    M.m[0][0] = 2.0f/(right-left);
    M.m[1][1] = 2.0f/(top-bottom);
    M.m[2][2] = -2.0f/(farp-nearp);
    M.m[0][3] = -(right+left)/(right-left);
    M.m[1][3] = -(top+bottom)/(top-bottom);
    M.m[2][3] = -(farp+nearp)/(farp-nearp);
    M.m[3][3] = 1.0f;
    return M;
}
Mat4 makePerspectiveMatrix(float fovDeg, float aspect, float nearp, float farp){
    float fovRad = fovDeg * M_PI/180.0f;
    float f = 1.0f / tanf(fovRad/2.0f);
    Mat4 P; memset(P.m,0,sizeof(P.m));
    P.m[0][0] = f / aspect;
    P.m[1][1] = f;
    P.m[2][2] = (farp+nearp)/(nearp-farp);
    P.m[2][3] = (2.0f*farp*nearp)/(nearp-farp);
    P.m[3][2] = -1.0f;
    return P;
}

// Viewport mapping NDC (-1..1) -> screen pixels
void ndcToScreen(const Vec4 &ndc, int &sx, int &sy){
    float x = ndc.x; float y = ndc.y;
    sx = (int)((x*0.5f + 0.5f) * (float)WINDOW_W + 0.5f);
    sy = (int)((y*0.5f + 0.5f) * (float)WINDOW_H + 0.5f);
}

// DDA: draw as GL_POINTS
void drawLineDDA(int x0, int y0, int x1, int y1){
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = std::max(std::abs(dx), std::abs(dy));
    if(steps == 0){
        glBegin(GL_POINTS); glVertex2i(x0,y0); glEnd();
        return;
    }
    float xInc = dx / (float)steps;
    float yInc = dy / (float)steps;
    float x = (float)x0, y = (float)y0;
    glBegin(GL_POINTS);
    for(int i=0;i<=steps;i++){
        glVertex2i((int)roundf(x),(int)roundf(y));
        x += xInc; y += yInc;
    }
    glEnd();
}

struct Object3D {
    std::vector<Vec4> verts;
    std::vector<std::pair<int,int>> edges;
};

Object3D cubeObj(){
    Object3D o;
    o.verts = {
        makeVec(-1,-1,-1), makeVec(1,-1,-1), makeVec(1,1,-1), makeVec(-1,1,-1),
        makeVec(-1,-1,1),  makeVec(1,-1,1),  makeVec(1,1,1),  makeVec(-1,1,1)
    };
    o.edges = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    return o;
}
Object3D pyramidObj(){
    Object3D o;
    o.verts = {
        makeVec(-1,-1,-1), makeVec(1,-1,-1), makeVec(1,1,-1), makeVec(-1,1,-1),
        makeVec(0,0,1.5f)
    };
    o.edges = {
        {0,1},{1,2},{2,3},{3,0}, {0,4},{1,4},{2,4},{3,4}
    };
    return o;
}

// Scene state
Object3D cube = cubeObj();
Object3D pyramid = pyramidObj();

Mat4 model = Mat4::identity();
bool showGrid = true;
bool usePerspective = true;
bool showHelp = true;
bool colorShading = true;
float fov = 60.0f;
float aspect = (float)WINDOW_W / (float)WINDOW_H;
float nearp = 0.1f, farp = 100.0f;

void drawString(float x, float y, const char *s){
    glRasterPos2f(x,y);
    while(*s) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *s++);
}

void renderObject(const Object3D &obj, const Mat4 &modelMatrix){
    Mat4 P;
    if(usePerspective) P = makePerspectiveMatrix(fov, aspect, nearp, farp);
    else {
        float s = 4.0f;
        P = makeOrthoMatrix(-s, s, -s*(1.0f/aspect), s*(1.0f/aspect), -50.0f, 50.0f);
    }

    auto outside = [](const Vec4 &v)->int{
        int bits=0;
        if(v.x < -1) bits |= 1;
        if(v.x >  1) bits |= 2;
        if(v.y < -1) bits |= 4;
        if(v.y >  1) bits |= 8;
        if(v.z < -1) bits |= 16;
        if(v.z >  1) bits |= 32;
        return bits;
    };

    for(const auto &e : obj.edges){
        Vec4 v0 = modelMatrix * obj.verts[e.first];
        Vec4 v1 = modelMatrix * obj.verts[e.second];

        Vec4 c0 = P * v0;
        Vec4 c1 = P * v1;

        // Avoid divide-by-zero
        if (c0.w == 0.0f || c1.w == 0.0f) continue;

        Vec4 ndc0 = { c0.x / c0.w, c0.y / c0.w, c0.z / c0.w, 1.0f };
        Vec4 ndc1 = { c1.x / c1.w, c1.y / c1.w, c1.z / c1.w, 1.0f };

        int o0 = outside(ndc0), o1 = outside(ndc1);
        if ((o0 & o1) != 0) continue;

        int sx0, sy0, sx1, sy1;
        ndcToScreen(ndc0, sx0, sy0);
        ndcToScreen(ndc1, sx1, sy1);

        float depth = 0.5f*(ndc0.z + ndc1.z);
        float shade = 1.0f - (depth + 1.0f)/2.0f;
        shade = std::max(0.0f, std::min(1.0f, shade));

        if(colorShading) glColor3f(0.2f + 0.8f*shade, 0.2f, 0.6f - 0.5f*(1.0f-shade));
        else glColor3f(1.0f,1.0f,1.0f);

        drawLineDDA(sx0, sy0, sx1, sy1);
    }
}

void display(){
    glClearColor(0.05f,0.06f,0.08f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 2D pixel-space projection so GL_POINTS map to screen pixels directly
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, WINDOW_W, 0, WINDOW_H, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    glEnable(GL_POINT_SMOOTH);
    glPointSize(1.6f);

    if(showGrid){
        glColor3f(0.18f,0.18f,0.18f);
        glBegin(GL_LINES);
        for(int gx=0; gx<=WINDOW_W; gx+=50){ glVertex2i(gx,0); glVertex2i(gx,WINDOW_H); }
        for(int gy=0; gy<=WINDOW_H; gy+=50){ glVertex2i(0,gy); glVertex2i(WINDOW_W,gy); }
        glEnd();
    }

    glColor3f(0.6f,0.6f,0.6f);
    drawLineDDA(0, WINDOW_H/2, WINDOW_W, WINDOW_H/2);
    drawLineDDA(WINDOW_W/2, 0, WINDOW_W/2, WINDOW_H);

    renderObject(cube, model * translate(-2.5f, 0.0f, -8.0f));
    renderObject(pyramid, model * translate(2.0f, -0.5f, -6.0f) * scaleM(0.9f,0.9f,0.9f));

    if(showHelp){
        glColor3f(1,1,1);
        drawString(10, WINDOW_H-18, "o: Ortho   p: Perspective   +/-: FOV  Arrow: translate XY  PgUp/PgDn: translate Z");
        drawString(10, WINDOW_H-36, "x/X,y/Y,z/Z: rotate  s/S: scale  r: reset  g: grid  c: color  h: help  ESC: exit");
        char buf[128];
        sprintf(buf, "Projection: %s   FOV: %.1f", usePerspective ? "Perspective" : "Orthographic", fov);
        drawString(10, WINDOW_H-54, buf);
    }

    glutSwapBuffers();
}

void specialKey(int key, int x, int y){
    (void)x; (void)y;
    float tstep = 0.2f;
    if(key==GLUT_KEY_LEFT)  model = translate(-tstep,0,0) * model;
    if(key==GLUT_KEY_RIGHT) model = translate( tstep,0,0) * model;
    if(key==GLUT_KEY_UP)    model = translate(0,tstep,0) * model;
    if(key==GLUT_KEY_DOWN)  model = translate(0,-tstep,0) * model;
    if(key==GLUT_KEY_PAGE_UP)   model = translate(0,0,tstep) * model;
    if(key==GLUT_KEY_PAGE_DOWN) model = translate(0,0,-tstep) * model;
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y){
    (void)x; (void)y;
    switch(key){
        case 27: exit(0); break;
        case 'o': usePerspective = false; break;
        case 'p': usePerspective = true; break;
        case '+': fov = std::max(10.0f, fov-2.0f); break;
        case '-': fov = std::min(150.0f, fov+2.0f); break;
        case 'x': model = rotateX(-5.0f) * model; break;
        case 'X': model = rotateX(5.0f) * model; break;
        case 'y': model = rotateY(-5.0f) * model; break;
        case 'Y': model = rotateY(5.0f) * model; break;
        case 'z': model = rotateZ(-5.0f) * model; break;
        case 'Z': model = rotateZ(5.0f) * model; break;
        case 's': model = scaleM(0.9f,0.9f,0.9f) * model; break;
        case 'S': model = scaleM(1.1f,1.1f,1.1f) * model; break;
        case 'r': model = Mat4::identity(); break;
        case 'g': showGrid = !showGrid; break;
        case 'c': colorShading = !colorShading; break;
        case 'h': showHelp = !showHelp; break;
        default: break;
    }
    glutPostRedisplay();
}

void reshape(int w, int h){
    (void)w; (void)h;
    glutPostRedisplay();
}

int main(int argc, char** argv){
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WINDOW_W, WINDOW_H);
    glutCreateWindow("Wireframe 3D with Software DDA + Transforms (fixed)");
    glViewport(0,0,WINDOW_W,WINDOW_H);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_POINT_SMOOTH);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKey);
    printf("Controls: o=ortho, p=persp, +/- fov, arrows translate XY, PgUp/PgDn translate Z, x/X y/Y z/Z rotate, s/S scale, r reset\n");
    glutMainLoop();
    return 0;
}
