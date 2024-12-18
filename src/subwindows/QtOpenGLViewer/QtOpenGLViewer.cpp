/* --------------------------------------------------------------------------------
 * Copyright 2018 by Marcel Paz Goldschen-Ohm <marcel.goldschen@gmail.com>
 * -------------------------------------------------------------------------------- */

#include "QtOpenGLViewer.h"

#include <climits>
#include <cmath>

#include <QKeyEvent>
#include <QtMath>
#include <QMatrix4x4>
#include <QMessageBox>
#include <QMouseEvent>
#include <QVector4D>
#include <QWheelEvent>

QVector3D QtOpenGLViewer::screen2World(QVector3D screen, int *viewport, float *projection, float *modelview)
{
    QMatrix4x4 P(projection);
    QMatrix4x4 M(modelview);
    P = P.transposed(); // row to column major order
    M = M.transposed(); // row to column major order
    double x = screen.x();
    double y = viewport[3] - screen.y();
    double z = screen.z();
    QVector4D in(
        2 * (x - viewport[0]) / viewport[2] - 1, // Map between (-1,1)
        2 * (y - viewport[1]) / viewport[3] - 1, // Map between (-1,1)
        2 *  z                              - 1, // Fed in as 0 or 1 which maps to (-1,1)
        1
    );
    QVector4D out = (P * M).inverted() * in;
    if(out[3] == 0)
        throw std::exception("QtOpenGLViewer::screen2World: Failed.");
    x = out[0] / out[3];
    y = out[1] / out[3];
    z = out[2] / out[3];
    return QVector3D(x, y, z);
}

QVector3D QtOpenGLViewer::world2Screen(QVector3D world, int *viewport, float *projection, float *modelview)
{
    QMatrix4x4 P(projection);
    QMatrix4x4 M(modelview);
    P = P.transposed(); // row to column major order
    M = M.transposed(); // row to column major order
    QVector4D A(world.x(), world.y(), world.z(), 1);
    QVector4D B = (M * P) * A;
    if(B[3] == 0)
        throw std::exception("QtOpenGLViewer::world2Screen: Failed.");
    B[0] /= B[3];
    B[1] /= B[3];
    B[2] /= B[3];
    double x = viewport[0] + ((B[0] + 1) * viewport[2]) / 2; // Map from (-1,1)
    double y = viewport[1] + ((B[1] + 1) * viewport[3]) / 2; // Map from (-1,1)
    double z = (1 + B[2]) / 2; // Map from (-1,1) to (0,1)
    return QVector3D(round(x), round(viewport[3] - y), z);
}

float QtOpenGLViewer::intersectRayAndSphere(const QVector3D &rayOrigin, const QVector3D &rayDirection, const QVector3D &sphereCenter, float sphereRadius)
{
    QVector3D L = sphereCenter - rayOrigin;
    float tca = QVector3D::dotProduct(L, rayDirection);
    if(tca < 0) return -1;
    float d2 = QVector3D::dotProduct(L, L) - (tca * tca);
    float r2 = sphereRadius * sphereRadius;
    if(d2 > r2) return -1;
    float thc = sqrt(r2 - d2);
    return tca - thc;
}

float QtOpenGLViewer::intersectRayAndPlane(const QVector3D &rayOrigin, const QVector3D &rayDirection, const QVector3D &pointOnPlane, const QVector3D &planeNormal)
{
    float numerator = QVector3D::dotProduct(planeNormal, pointOnPlane - rayOrigin);
    float denominator = QVector3D::dotProduct(planeNormal, rayDirection);
    return (fabs(denominator) > 1e-5 ? numerator / denominator : -1);
}

void QtOpenGLViewer::goToBillboard(const QVector3D &origin, const QVector3D &right)
{
    QVector3D xhat = (right - origin).normalized();
    QVector3D yhat = QVector3D::crossProduct(xhat, camera.view()).normalized();
    QVector3D zhat = QVector3D::crossProduct(xhat, yhat).normalized();
    // Rotation to (x, y, z) and translation to origin.
    float transform[16] = {
        xhat.x(), xhat.y(), xhat.z(), 0,
        yhat.x(), yhat.y(), yhat.z(), 0,
        zhat.x(), zhat.y(), zhat.z(), 0,
        origin.x(), origin.y(), origin.z(), 1
    };
    glMultMatrixf(transform);
}

bool QtOpenGLViewer::isLeftToRight(const QVector3D &vec)
{
    QVector3D right = QVector3D::crossProduct(camera.view(), camera.up);
    float dpr = QVector3D::dotProduct(right.normalized(), vec);
    float len = vec.length();
    float epsilon = 1e-5 * len;
    if(fabs(dpr) < epsilon) {
        // straight up-down
        float dpu = QVector3D::dotProduct(camera.up.normalized(), vec);
        if(-dpu > len - epsilon) {
            return false; // straight down
        } else {
            return true; // straight up
        }
    }
    return dpr > 0;
}

float QtOpenGLViewer::luminance(const QColor &color)
{
    float r = color.redF();
    float g = color.greenF();
    float b = color.blueF();
    if(r <= 0.03928) r /= 12.92; else r = pow((r + 0.055) / 1.055, 2.4);
    if(g <= 0.03928) g /= 12.92; else g = pow((g + 0.055) / 1.055, 2.4);
    if(b <= 0.03928) b /= 12.92; else b = pow((b + 0.055) / 1.055, 2.4);
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

QColor QtOpenGLViewer::colorWithMaxContrast(const QColor &color)
{
    float hue = color.hueF() + 0.5;
    if(hue > 1) hue -= 1;
    return QColor::fromHslF(hue, 1, 0.5);
}

void QtOpenGLViewer::renderText(float x, float y, const QString &text, const QColor &color, const QFont &font)
{
    // QPainter changes one or more attributes so all relevent ones are pushed then popped on end of QPainter.
    glPushAttrib(GL_ACCUM_BUFFER_BIT);
    glPushAttrib(GL_VIEWPORT_BIT);
    glPushAttrib(GL_TRANSFORM_BIT);
    glPushAttrib(GL_POLYGON_BIT);
    glPushAttrib(GL_PIXEL_MODE_BIT);
    glPushAttrib(GL_MULTISAMPLE_BIT);
    glPushAttrib(GL_LIGHTING_BIT);
    glPushAttrib(GL_ENABLE_BIT);
    glPushAttrib(GL_DEPTH_BUFFER_BIT);
    glPushAttrib(GL_CURRENT_BIT);
    glPushAttrib(GL_COLOR_BUFFER_BIT);
    QPainter painter(this);
    painter.setPen(color);
    painter.setFont(font);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.drawText(x, y, text);
    painter.end();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
}

void QtOpenGLViewer::drawComponentsRecursively(Component *component) {

    if(component->getType() == CAMERA) {
        auto c = dynamic_cast<CameraComponent*>(component);
        createCuboidAt(c->dimX, c->dimY, c->dimZ, c->locX, c->locY, c->locZ, c->rotX, c->rotY, c->rotZ);
    } else if(component->getType() == SENSOR) {
        auto c = dynamic_cast<SensorComponent*>(component);
        createCuboidAt(c->dimX, c->dimY, c->dimZ, c->locX, c->locY, c->locZ, c->rotX, c->rotY, c->rotZ);
    } else if(component->getType() == LENS) {
        auto c = dynamic_cast<LensComponent*>(component);
        createCylinderAt(c->dimX, c->dimY, c->dimZ, c->locX, c->locY, c->locZ, c->rotX, c->rotY, c->rotZ);
    } else if(component->getType() == ILLUMINATOR) {
        auto c = dynamic_cast<IlluminatorComponent*>(component);
        createCuboidAt(c->dimX, c->dimY, c->dimZ, c->locX, c->locY, c->locZ, c->rotX, c->rotY, c->rotZ);
    } else if(component->getType() == FILTER) {
        auto c = dynamic_cast<FilterComponent*>(component);
        createCylinderAt(c->dimX, c->dimY, c->dimZ, c->locX, c->locY, c->locZ, c->rotX, c->rotY, c->rotZ);
    } else if(component->getType() == SCREEN) {
        auto c = dynamic_cast<ScreenComponent*>(component);
        createCuboidAt(c->dimX, c->dimY, c->dimZ, c->locX, c->locY, c->locZ, c->rotX, c->rotY, c->rotZ);
    } else if(component->getType() == CVTARGET) {
        auto c = dynamic_cast<CvTargetComponent*>(component);
        createCylinderAt(c->dimX, c->dimY, c->dimZ, c->locX, c->locY, c->locZ, c->rotX, c->rotY, c->rotZ);
    } else if(component->getType() == EYEBALL) {
        auto c = dynamic_cast<EyeballComponent*>(component);
        createSpheroidAt(c->dimX, c->dimY, c->dimZ, c->locX, c->locY, c->locZ, c->rotX, c->rotY, c->rotZ);
    }

    for(int j = 0; j < component->components.size(); j++) {
        drawComponentsRecursively(component->components[j]);
    }
};

void QtOpenGLViewer::drawScene()
{
    /*
    createCylinderAt(1,1,2, 1,4,2, 75,10,16);
    //createCube();
    //createCuboidAt();
    createCuboidAt(2.5, 0.5, 0.5, 3,2,2, 45,30,20);
    //createCylinder();
    //createCone();
    createConeAt(1.0, 1.0, 1.0, -1,-1,1, 15,20,10);

    createSpheroidAt(1.0, 3.0, 3.0, -5,-2,1, 90,45,45);
    */

    drawAxes();

    if(!setupModel || !setupModel->isInitialized() || !setupModel->isValid())
        return;

    for(int i = 0; i < setupModel->cameraUnits.size(); i++) {
        for(int j = 0; j < setupModel->cameraUnits[i]->components.size(); j++) {
            drawComponentsRecursively(setupModel->cameraUnits[i]->components[j]);
        }
    }
    for(int i = 0; i < setupModel->illuminatorUnits.size(); i++) {
        for(int j = 0; j < setupModel->illuminatorUnits[i]->components.size(); j++) {
            drawComponentsRecursively(setupModel->illuminatorUnits[i]->components[j]);
        }
    }
    for(int i = 0; i < setupModel->screenUnits.size(); i++) {
        for(int j = 0; j < setupModel->screenUnits[i]->components.size(); j++) {
            drawComponentsRecursively(setupModel->screenUnits[i]->components[j]);
        }
    }
    for(int i = 0; i < setupModel->heads.size(); i++) {
        for(int j = 0; j < setupModel->heads[i]->components.size(); j++) {
            drawComponentsRecursively(setupModel->heads[i]->components[j]);
        }
    }

    // There should rather be getters, e.g. getLeftEyeball
    //      és ezekből előteremtve a locX, locY, locZ értékeket, vonalat lehetne húzni a szemből a képernyőre merőlegesen, stb
}

void QtOpenGLViewer::drawHud(QPainter &painter)
{
    QString text;
    if(is3D()) text = "Mouse: M=pan, R=rot, W=zoom";
    else text = "Mouse: M,R=pan, W=zoom";
    if(!text.isEmpty()) {
        QFontMetricsF fm(_hudFont);
        QRectF bbox = fm.tightBoundingRect(text);
        float x = bbox.bottomLeft().x();
        float y = bbox.bottomLeft().y();
        float h = bbox.height();
        QColor textColor = luminance(_backgroundColor) > 0.25 ? QColor(0, 0, 0) : QColor(255, 255, 255); // WC3 guidlines is L > ~0.179
        painter.setPen(textColor);
        painter.setFont(_hudFont);
        painter.drawText(2 - x, 2 - y + h, text);
    }
}

//
/*
// Used from Qt forum answer by user PthonDuncan: https://forum.qt.io/post/775714, Last accessed: 2024.10.26. 07:56 CET
void QtOpenGLViewer::qt_save_gl_state()
{
    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glShadeModel(GL_FLAT);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

// Used from Qt forum answer by user PthonDuncan: https://forum.qt.io/post/775714, Last accessed: 2024.10.26. 07:56 CET
void QtOpenGLViewer::qt_restore_gl_state()
{
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    glPopClientAttrib();
}

// Used from Qt forum answer by user PthonDuncan: https://forum.qt.io/post/775714, Last accessed: 2024.10.26. 07:56 CET
void QtOpenGLViewer::renderText(double x, double y, const QString text)
{
    GLdouble textPosX = x, textPosY = y;
    // Retrieve last OpenGL color to use as a font color
    GLdouble glColor[4];
    glGetDoublev(GL_CURRENT_COLOR, glColor);
    QColor fontColor = QColor(glColor[0]*255, glColor[1]*255,
                              glColor[2]*255, glColor[3]*255);
    // Render text
    QPainter painter(this);
//    painter.translate(float(_shiftX),float(_shiftY)); //This is for my own mouse event (scaling)

    painter.setPen(fontColor);
    QFont f;
    f.setPixelSize(10);
    painter.setFont(f);
    painter.drawText(textPosX, textPosY, text);
    painter.end();
}
 */
//

//////////////////////////////////////////////////////////////////////////

// From SO post by user jaba: https://stackoverflow.com/a/33674071/11414500, Last accessed: 2024.10.26. 08:12 CET
inline GLint QtOpenGLViewer::project(GLdouble objx, GLdouble objy, GLdouble objz,
                            const GLdouble model[16], const GLdouble proj[16],
                            const GLint viewport[4],
                            GLdouble * winx, GLdouble * winy, GLdouble * winz)
{
    GLdouble in[4], out[4];

    in[0] = objx;
    in[1] = objy;
    in[2] = objz;
    in[3] = 1.0;
    transformPoint(out, model, in);
    transformPoint(in, proj, out);

    if (in[3] == 0.0)
        return GL_FALSE;

    in[0] /= in[3];
    in[1] /= in[3];
    in[2] /= in[3];

    *winx = viewport[0] + (1 + in[0]) * viewport[2] / 2;
    *winy = viewport[1] + (1 + in[1]) * viewport[3] / 2;

    *winz = (1 + in[2]) / 2;
    return GL_TRUE;
}

// From SO post by user jaba: https://stackoverflow.com/a/33674071/11414500, Last accessed: 2024.10.26. 08:12 CET
void QtOpenGLViewer::renderText(GLdouble objx, GLdouble objy, GLdouble objz, QString text, QColor color)
{
    int width = this->width();
    int height = this->height();

    GLdouble model[4][4], proj[4][4];
    GLint view[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, &model[0][0]);
    glGetDoublev(GL_PROJECTION_MATRIX, &proj[0][0]);
    glGetIntegerv(GL_VIEWPORT, &view[0]);
    GLdouble textPosX = 0, textPosY = 0, textPosZ = 0;

    project(objx, objy, objz,
            &model[0][0], &proj[0][0], &view[0],
            &textPosX, &textPosY, &textPosZ);

    textPosY = height - textPosY; // y is inverted

    QPainter painter(this);
    painter.setPen(color);
    painter.setFont(QFont("Helvetica", 8));
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.drawText(textPosX, textPosY, text); // z = pointT4.z + distOverOp / 4
    painter.end();
}

// From SO post by user jaba: https://stackoverflow.com/a/33674071/11414500, Last accessed: 2024.10.26. 08:12 CET
inline void QtOpenGLViewer::transformPoint(GLdouble out[4], const GLdouble m[16], const GLdouble in[4])
{
#define M(row,col)  m[col*4+row]
    out[0] = M(0, 0) * in[0] + M(0, 1) * in[1] + M(0, 2) * in[2] + M(0, 3) * in[3];
    out[1] = M(1, 0) * in[0] + M(1, 1) * in[1] + M(1, 2) * in[2] + M(1, 3) * in[3];
    out[2] = M(2, 0) * in[0] + M(2, 1) * in[1] + M(2, 2) * in[2] + M(2, 3) * in[3];
    out[3] = M(3, 0) * in[0] + M(3, 1) * in[1] + M(3, 2) * in[2] + M(3, 3) * in[3];
#undef M
}
///////////////////////////////////////////////////////////

void QtOpenGLViewer::drawAxes()
{
    glPushAttrib(GL_COLOR_BUFFER_BIT);
    glPushAttrib(GL_LINE_BIT);
    glLineWidth(4);
    glBegin(GL_LINES);
    // x
    //glColor3f(1, 0, 0);
    glColor3f(252.0f/255, 80.0f/255, 0.0f/255);
    glVertex3f(0, 0, 0);
    glVertex3f(1, 0, 0);
    // y
    //glColor3f(0, 1, 0);
    glColor3f(139.0f/255, 252.0f/255, 0.0f/255);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 1, 0);
    // z
    //glColor3f(0, 0, 1);
    glColor3f(0.0f/255, 160.0f/255, 255.0f/255);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, 1);
    glEnd();
    glPopAttrib(); // GL_LINE_BIT
    glPopAttrib(); // GL_COLOR_BUFFER_BIT

    // Also mark axes with letters for clarity
    renderText(1.1, 0, 0, "X", QColor::fromRgb(252, 80, 0));
    renderText(0, 1.1, 0, "Y", QColor::fromRgb(139, 252, 0));
    renderText(0, 0, 1.1, "Z", QColor::fromRgb(0, 160, 255));

    /////////////////////////////
    // SIMA 2D SZÖVEGET ÍR A TERÜLETRE, AMI NEM MOZOG
    /*
    QPainter painter(this);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 16));
    painter.drawText(0, 0, width(), height(), Qt::AlignCenter, "Hello World!");
    painter.end();
    */

//    renderText(10, 10, 10, "Hahaha");
}

void QtOpenGLViewer::selectObject(const QPoint &mousePosition)
{
//    QVector3D pickOrigin, pickRay;
//    getPickRay(mousePosition, pickOrigin, pickRay);
//    _selectedObject = NULL;
    // find object with closest intersection to pick ray...
}

void QtOpenGLViewer::getPickRay(const QPoint &mousePosition, QVector3D &origin, QVector3D &ray)
{
    makeCurrent();
    int viewport[4] = {0, 0, width(), height()};
    float projection[16];
    float modelview[16];
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    origin = screen2World(QVector3D(mousePosition.x(), mousePosition.y(), 0), viewport, projection, modelview);
    ray = screen2World(QVector3D(mousePosition.x(), mousePosition.y(), 1), viewport, projection, modelview) - origin;
}

QVector3D QtOpenGLViewer::pickPointInPlane(const QPoint &mousePosition, const QVector3D &pointOnPlane, bool snapToUnitGrid)
{
    QVector3D pickOrigin, pickRay;
    getPickRay(mousePosition, pickOrigin, pickRay);
    pickRay.normalize();
    float t = intersectRayAndPlane(pickOrigin, pickRay, pointOnPlane, camera.view().normalized());
    if(t >= 0) {
        QVector3D pt = pickOrigin + (pickRay * t);
        if(snapToUnitGrid) {
            pt.setX(round(pt.x()));
            pt.setY(round(pt.y()));
            pt.setZ(round(pt.z()));
        }
        return pt;
    }
    return pointOnPlane;
}

void QtOpenGLViewer::goToDefaultView(int viewNumber)
{
    switch(viewNumber) {
        case 1:
            // XY plane
            camera.eye = QVector3D(0, 0, 10);
            camera.center = QVector3D(0, 0, 0);
            camera.up = QVector3D(0, 1, 0);
            break;
        case 2:
            // ZY plane
            camera.eye = QVector3D(10, 0, 0);
            camera.center = QVector3D(0, 0, 0);
            camera.up = QVector3D(0, 1, 0);
            break;
        case 3:
            // ZX plane
            camera.eye = QVector3D(0, 10, 0);
            camera.center = QVector3D(0, 0, 0);
            camera.up = QVector3D(0, 0, -1);
            break;
        case 4:
            // 3D from the inside
            camera.eye = QVector3D(qSqrt(3)*10, qSqrt(3)*10, qSqrt(3)*10);
            camera.center = QVector3D(0, 0, 0);
            camera.up = QVector3D(0, 1, 0);
            break;
    }
    repaint();
}

void QtOpenGLViewer::deleteSelectedObject()
{
    if(!_selectedObject) return;
    QString title("Delete selected object?");
    QString text("Delete " + _selectedObject->objectName() + "?");
    if(QMessageBox::question(this, title, text, QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) return;
    delete _selectedObject;
    _selectedObject = NULL;
    emit selectedObjectChanged(_selectedObject);
    repaint();
}

void QtOpenGLViewer::editSelectedObject(const QPoint &mousePosition)
{
    return;
    // this doesn't really do anything useful, but provides a simple example of how you might popup an editor widget for your scene objects
    if(!_selectedObject) return;
    QWidget *editor = new QWidget;
    editor->setWindowModality(Qt::ApplicationModal);
    editor->setWindowTitle(_selectedObject->metaObject()->className());
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->show();
}

void QtOpenGLViewer::initializeGL()
{
    // ALAPBÓL ENNYI VOLT:
    initializeOpenGLFunctions();

    /*
    // A GL DEBUG MIATT ILYEN LETT:
     // DE: nem jó, mert ez a format itt nem változtatható meg, hanem ennek a widgetnek a szülőjében lehet ha jól értem.
     //     Akkor viszont semmi nem fog rendesen kirajzolódni a widgetben, bár nem írja ki a qt hogy nincs bekapcsolva a debug kimenet a gl-ben
    QSurfaceFormat format;
// asks for a OpenGL 3.2 debug context using the Core profile
    format.setMajorVersion(4);
    format.setMinorVersion(5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setOption(QSurfaceFormat::DebugContext);

    setFormat(format);
    create();
    initializeOpenGLFunctions();
    */
/*
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    Q_ASSERT(ctx->hasExtension(QByteArrayLiteral("GL_KHR_debug")));
    QOpenGLDebugLogger *logger = new QOpenGLDebugLogger(this);

    logger->initialize(); // initializes in the current context, i.e. ctx
*/

    //f = QOpenGLContext::currentContext()->functions();
//    f = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_4_5_Core>();
    glClearColor(_backgroundColor.redF(), _backgroundColor.greenF(), _backgroundColor.blueF(), _backgroundColor.alphaF());
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    //glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHT0);
    glDisable(GL_LIGHTING);
}

void QtOpenGLViewer::resizeGL(int /* w */, int /* h */)
{
    glViewport(0, 0, width(), height());
    
    // ortho projection is handled in paintGL()
    
//    // perspective projection
//    glMatrixMode(GL_PROJECTION);
//    glLoadIdentity();
//    float fovy = 20 * M_PI / 180;
//    float aspect = float(width()) / height();
//    float near = 0.1;
//    float far = 200;
//    float f = 1 / tan(fovy / 2);
//    float perspective[16] = {
//        f/aspect, 0,                     0,  0,
//        0,        f,                     0,  0,
//        0,        0, (far+near)/(near-far), -1,
//        0,        0, 2*far*near/(near-far),  0
//    };
//    glLoadMatrixf(perspective);
//    glMatrixMode(GL_MODELVIEW);
//    glLoadIdentity();
}

void QtOpenGLViewer::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_CLAMP); // THIS REMOVES CLIPPING when camera is too close to the shapes

    // ortho projection (call here to handle zoom)
    // ortho box is sized relative to zoom distance
    float m_zoom = camera.view().length();
    float m_left = -m_zoom / 2;
    float m_right = m_zoom / 2;
    float m_bottom = -m_zoom / 2;
    float m_top = m_zoom / 2;
    float m_near = m_zoom / 100;
    float m_far = m_zoom * 2;
    float m_aspect = float(width()) / height();
    if(m_aspect < 1) {
        m_bottom /= m_aspect;
        m_top /= m_aspect;
    } else if(m_aspect > 1) {
        m_left *= m_aspect;
        m_right *= m_aspect;
    }
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(m_left, m_right, m_bottom, m_top, m_near, m_far);

    // lookat
    QVector3D zhat = -camera.view().normalized();
    QVector3D xhat = QVector3D::crossProduct(camera.up, zhat).normalized();
    QVector3D yhat = QVector3D::crossProduct(zhat, xhat).normalized();
    float eyeX = QVector3D::dotProduct(camera.eye, xhat);
    float eyeY = QVector3D::dotProduct(camera.eye, yhat);
    float eyeZ = QVector3D::dotProduct(camera.eye, zhat);
    float lookat[16] = {
        xhat.x(), yhat.x(), zhat.x(), 0,
        xhat.y(), yhat.y(), zhat.y(), 0,
        xhat.z(), yhat.z(), zhat.z(), 0,
        -eyeX,    -eyeY,    -eyeZ,    1
    };
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(lookat);

    // scene
    drawScene();
    
    // hud
    // QPainter changes one or more attributes so all relevant ones are pushed then popped on end of QPainter.
    // not sure if all of this is really needed?
    glPushAttrib(GL_ACCUM_BUFFER_BIT);
    glPushAttrib(GL_VIEWPORT_BIT);
    glPushAttrib(GL_TRANSFORM_BIT);
    glPushAttrib(GL_POLYGON_BIT);
    glPushAttrib(GL_PIXEL_MODE_BIT);
    glPushAttrib(GL_MULTISAMPLE_BIT);
    glPushAttrib(GL_LIGHTING_BIT);
    glPushAttrib(GL_ENABLE_BIT);
    glPushAttrib(GL_DEPTH_BUFFER_BIT);
    glPushAttrib(GL_CURRENT_BIT);
    glPushAttrib(GL_COLOR_BUFFER_BIT);
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    drawHud(painter);
    painter.end();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
}

void QtOpenGLViewer::keyPressEvent(QKeyEvent *event) {

    qDebug() << "keyPressEvent in QtOpenGLViewer";

    switch(event->key()) {
        case Qt::Key_Backspace:
        case Qt::Key_Delete:
            qDebug() << "delete key";
            deleteSelectedObject();
            return;
            
        case Qt::Key_1:
            goToDefaultView(1);
            return;
        case Qt::Key_2:
            goToDefaultView(2);
            return;
        case Qt::Key_3:
            goToDefaultView(3);
            return;
        case Qt::Key_4:
            goToDefaultView(4);
            return;
    }
    QOpenGLWidget::keyPressEvent(event);
}

void QtOpenGLViewer::mousePressEvent(QMouseEvent *event) {

    qDebug() << "mousePressEvent in QtOpenGLViewer";

    if(event->button() == Qt::LeftButton) {
        // object selection
        QObject *prevSelectedObject = _selectedObject;
        selectObject(event->pos());
        if(_selectedObject != prevSelectedObject) {
            emit selectedObjectChanged(_selectedObject);
        }
        if(_selectedObject) {
            // for dragging object
            _mousePosition = event->pos();
            setMouseTracking(true);
        }
        repaint();
        return;
    } else if(event->button() == Qt::MiddleButton || (!is3D() && (event->button() == Qt::RightButton))) {
        // pan
        _mousePosition = event->pos();
        setMouseTracking(true);
        return;
    } else if(event->button() == Qt::RightButton) {
        if(is3D()) {
            // rotate
            _mousePosition = event->pos();
            setMouseTracking(true);
            return;
        }
    }
    QOpenGLWidget::mousePressEvent(event);
}

void QtOpenGLViewer::mouseReleaseEvent(QMouseEvent *event)
{
    if(true) {
        setMouseTracking(false);
        return;
    }
    QOpenGLWidget::mousePressEvent(event);
}

void QtOpenGLViewer::mouseMoveEvent(QMouseEvent *event)
{
    // Transform scene.
    bool rotate = is3D() && (event->buttons() & Qt::RightButton);
    bool pan = event->buttons() & Qt::MiddleButton || (!is3D() && (event->buttons() & Qt::RightButton));
    if(rotate || pan) {
        float dx = event->x() - _mousePosition.x();
        float dy = -(event->y() - _mousePosition.y());
        _mousePosition = event->pos();
        QVector3D zhat = -camera.view().normalized();
        QVector3D xhat = QVector3D::crossProduct(camera.up, zhat).normalized();
        QVector3D yhat = QVector3D::crossProduct(zhat, xhat).normalized();
        if(rotate) {
            QVector3D rotationAxis = yhat * dx - xhat * dy;
            float n = rotationAxis.length();
            if(n > 1e-5) {
                rotationAxis.normalize();
                float radians = n / (float)width() * M_PI;
                // Rotate eye about center around rotation axis.
                float a = cos(radians / 2.0f);
                float s = -sin(radians / 2.0f);
                float b = rotationAxis.x() * s;
                float c = rotationAxis.y() * s;
                float d = rotationAxis.z() * s;
                float rotation[9] = {
                    a*a+b*b-c*c-d*d,    2.0f*(b*c-a*d),     2.0f*(b*d+a*c),
                    2.0f*(b*c+a*d),     a*a+c*c-b*b-d*d,    2.0f*(c*d-a*b),
                    2.0f*(b*d-a*c),     2.0f*(c*d+a*b),     a*a+d*d-b*b-c*c
                };
                camera.eye -= camera.center; // Shift center to origin so can rotate eye about axis through the origin.
                // Rotate eye around rotation axis.
                float ex = camera.eye.x() * rotation[0] + camera.eye.y() * rotation[1] + camera.eye.z() * rotation[2];
                float ey = camera.eye.x() * rotation[3] + camera.eye.y() * rotation[4] + camera.eye.z() * rotation[5];
                float ez = camera.eye.x() * rotation[6] + camera.eye.y() * rotation[7] + camera.eye.z() * rotation[8];
                camera.eye = QVector3D(ex, ey, ez);
                camera.eye += camera.center; // shift back to center.
                repaint();
                return;
            }
        } else if(pan) {
            float zoom = camera.view().length();
            QVector3D translation = xhat * (dx / (float)width() * zoom) + yhat * (dy / (float)height() * zoom);
            camera.center -= translation;
            camera.eye -= translation;
            repaint();
            return;
        }
    } // rotate || pan
    QOpenGLWidget::mouseMoveEvent(event);
}

void QtOpenGLViewer::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION >= 0x050000
    float degrees = event->angleDelta().y() / 8;
#else
    float degrees = event->delta() / 8;
#endif
    float steps = degrees / 15;  // Most mouse types work in steps of 15 degrees.
    if(steps == 0) return;
    if(swapMouseWheelZoomDirection()) {
        steps = -steps;
    }
    float zoom = camera.view().length();
    zoom += zoom * steps * mouseWheelSensitivity();
    if(zoom < 1e-5) {
        zoom = 1e-5;
    }
    camera.zoom(zoom);
    repaint();
}

void QtOpenGLViewer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton) {
        selectObject(event->pos());
        if(_selectedObject) {
            editSelectedObject(event->pos());
            return;
        }
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

// Used code from SO post by user CodeSurgeon: https://stackoverflow.com/a/41917591/11414500, Last accessed: 2024.10.27. 18:51 CET
void QtOpenGLViewer::createCylinder(float r, float h, float n) {
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glColor3ub(255,255,0); // bright yellow

    std::vector<QPointF> circle_pts;
    for(int i=0; i<n+1; i++) {
        float angle = 2.0f * M_PI * ((float)i / n);
        float x = r * qCos(angle);
        float y = r * qSin(angle);
        circle_pts.push_back(QPointF(x, y));
    }

    glBegin(GL_TRIANGLE_FAN); // drawing the back circle
    //glColor3f(1, 0, 0);
    glVertex3f(0, 0, h/2.0f);
    for(int i=0; i<circle_pts.size(); i++) {
        float z = h / 2.0f;
        glVertex3f(circle_pts[i].x(), circle_pts[i].y(), z);
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN); // drawing the front circle
    //glColor3f(0, 0, 1);
    glVertex3f(0, 0, -h/2.0f);
    for(int i=0; i<circle_pts.size(); i++) {
        float z = -h / 2.0f;
        glVertex3f(circle_pts[i].x(), circle_pts[i].y(), z);
    }
    glEnd();

    //glBegin(GL_TRIANGLE_STRIP); // draw the tube
    glBegin(GL_QUADS); // draw the tube
    //glColor3f(0, 1, 0);
    for(int i=0; i<circle_pts.size(); i++) {
        float z = h / 2.0f;
        glVertex3f(circle_pts[i].x(), circle_pts[i].y(), z);
        glVertex3f(circle_pts[i].x(), circle_pts[i].y(), -z);
    }
    glEnd();

    glColor3ub(255,255,255);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
}

// Used code from SO post by user CodeSurgeon: https://stackoverflow.com/a/41917591/11414500, Last accessed: 2024.10.27. 18:51 CET
void QtOpenGLViewer::createCone(float r, float h, float n) {
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glColor3ub(255,255,0); // bright yellow

    std::vector<QPointF> circle_pts;
    for(int i=0; i<n+1; i++) {
        float angle = 2.0f * M_PI * ((float)i / n);
        float x = r * qCos(angle);
        float y = r * qSin(angle);
        circle_pts.push_back(QPointF(x, y));
    }

    //glBegin(GL_TRIANGLE_STRIP); // drawing the back circle
    glBegin(GL_TRIANGLE_FAN); // drawing the back circle
    //glColor3f(1, 0, 0);
    glVertex3f(0, 0, h/2.0f);
    for(int i=0; i<circle_pts.size(); i++) {
        float z = h / 2.0f;
        glVertex3f(circle_pts[i].x(), circle_pts[i].y(), z);
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN); // drawing the sides
    //glColor3f(0, 0, 1);
    glVertex3f(0, 0, -h/2.0f);
    for(int i=0; i<circle_pts.size(); i++) {
        float z = h / 2.0f;
        glVertex3f(circle_pts[i].x(), circle_pts[i].y(), z);
    }
    glEnd();

    glColor3ub(255,255,255);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
}

void QtOpenGLViewer::createCube(GLfloat a) {

    a /= 2.0;

    //glLineWidth(2);
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glColor3ub(255,255,0); // bright yellow

    glBegin(GL_QUADS);
    
    glVertex3f(a,a,a);
    glVertex3f(-a,a,a);
    glVertex3f(-a,-a,a);
    glVertex3f(a,-a,a);
    
    glVertex3f(a,a,-a);
    glVertex3f(-a,a,-a);
    glVertex3f(-a,-a,-a);
    glVertex3f(a,-a,-a);
    
    glVertex3f(a,a,a);
    glVertex3f(a,-a,a);
    glVertex3f(a,-a,-a);
    glVertex3f(a,a,-a);
    
    glVertex3f(-a,a,a);
    glVertex3f(-a,-a,a);
    glVertex3f(-a,-a,-a);
    glVertex3f(-a,a,-a);
    
    glVertex3f(a,a,a);
    glVertex3f(-a,a,a);
    glVertex3f(-a,a,-a);
    glVertex3f(a,a,-a);
    
    glVertex3f(a,-a,a);
    glVertex3f(-a,-a,a);
    glVertex3f(-a,-a,-a);
    glVertex3f(a,-a,-a);
    
    glEnd();
    glColor3ub(255,255,255);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
}

// Used from SO post by user Max Collao: https://stackoverflow.com/a/30030112/11414500 Last accessed 2024.11.04. 09:10 CET
void QtOpenGLViewer::createSphere(float r, int nParal, int nMerid){

    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glColor3ub(255,255,0); // bright yellow

    float x,y,z,i,j;
    for (j=0;j<M_PI; j+=M_PI/(nParal+1)){
        glBegin(GL_LINE_LOOP);
        y=(float) (r*qCos(j));
        for(i=0; i<2*M_PI; i+=M_PI/60){
            x=(float) (r*qCos(i)*qSin(j));
            z=(float) (r*qSin(i)*qSin(j));
            glVertex3f(x,y,z);
        }
        glEnd();
    }

    for(j=0; j<M_PI; j+=M_PI/nMerid){
        glBegin(GL_LINE_LOOP);
        for(i=0; i<2*M_PI; i+=M_PI/60){
            x=(float) (r*qSin(i)*qCos(j));
            y=(float) (r*qCos(i));
            z=(float) (r*qSin(j)*qSin(i));
            glVertex3f(x,y,z);
        }
        glEnd();
    }
    glColor3ub(255,255,255);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
}

void QtOpenGLViewer::createCylinderAt(GLfloat dimX, GLfloat dimY, GLfloat dimZ, GLfloat locX, GLfloat locY, GLfloat locZ, GLfloat rotX, GLfloat rotY, GLfloat rotZ) {
    startTransformed(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ);
    createCylinder(0.5, 1.0, 25.0);
    endTransformed(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ);
}

void QtOpenGLViewer::createConeAt(GLfloat dimX, GLfloat dimY, GLfloat dimZ, GLfloat locX, GLfloat locY, GLfloat locZ, GLfloat rotX, GLfloat rotY, GLfloat rotZ) {
    startTransformed(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ);
    createCone(0.5, 1.0, 25.0);
    endTransformed(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ);
}

void QtOpenGLViewer::createCuboidAt(GLfloat dimX, GLfloat dimY, GLfloat dimZ, GLfloat locX, GLfloat locY, GLfloat locZ, GLfloat rotX, GLfloat rotY, GLfloat rotZ) {
    startTransformed(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ);
    createCube(1.0);
    endTransformed(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ);
}

void QtOpenGLViewer::createSpheroidAt(GLfloat dimX, GLfloat dimY, GLfloat dimZ, GLfloat locX, GLfloat locY, GLfloat locZ, GLfloat rotX, GLfloat rotY, GLfloat rotZ) {
    startTransformed(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ);
    createSphere(0.5, 10, 10);
    endTransformed(dimX, dimY, dimZ, locX, locY, locZ, rotX, rotY, rotZ);
}

// 1. a glScalef szoroz, tehát nincs abszolút imerete az előbbi állapotról. 1/3 szorosra kell állítani
//      ha egy korábbi 3x-os szorzást vissza akarunk állítani (nem pedig ..scale(1,1,1) hívás kell)
// 2. a glBegin előtt, és a glEnd után kell egy oda és vissza transzformálás. Oda hogy a rajzolás
//      transzformálva történjen, és vissza, hogy a legközelebbi rajzolás ne oda transzformálva folytatódjon
// 3. a sorrend a programkód sorrendjében történik a transzformációkor, tehát nem visszafelé halad a stacken
//      viszont épp ezért a glEnd után fordított sorrendben van szükség a visszatranszformálás hívásaira
void QtOpenGLViewer::startTransformed(GLfloat dimX, GLfloat dimY, GLfloat dimZ, GLfloat locX, GLfloat locY, GLfloat locZ, GLfloat rotX, GLfloat rotY, GLfloat rotZ) {
    glMatrixMode(GL_MODELVIEW);
    glRotatef(rotX, 1.0f, 0.0f, 0.0f);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glRotatef(rotZ, 0.0f, 0.0f, 1.0f);
    glTranslatef(locX, locY, locZ);
    glScalef(dimX, dimY, dimZ);
    glMatrixMode(GL_PROJECTION);
    //glGetError(); // not in qt gl
}
void QtOpenGLViewer::endTransformed(GLfloat dimX, GLfloat dimY, GLfloat dimZ, GLfloat locX, GLfloat locY, GLfloat locZ, GLfloat rotX, GLfloat rotY, GLfloat rotZ) {
    glMatrixMode(GL_MODELVIEW);
    glScalef(1.0f/dimX, 1.0f/dimY, 1.0f/dimZ);
    glTranslatef(-locX, -locY, -locZ);
    glRotatef(rotZ, 0.0f, 0.0f, -1.0f);
    glRotatef(rotY, 0.0f, -1.0f, 0.0f);
    glRotatef(rotX, -1.0f, 0.0f, 0.0f);
    glMatrixMode(GL_PROJECTION);
    //glGetError(); // not in qt gl
}
/*
void QtOpenGLViewer::startTransformed(GLfloat dimX, GLfloat dimY, GLfloat dimZ, GLfloat locX, GLfloat locY, GLfloat locZ, GLfloat rotX, GLfloat rotY, GLfloat rotZ) {
    glMatrixMode(GL_MODELVIEW);
    glRotatef(rotX, 1.0f, 0.0f, 0.0f);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glRotatef(rotZ, 0.0f, 0.0f, 1.0f);
    glTranslatef(locX, locY, locZ);
    glScalef(dimX, dimY, dimZ);
    glMatrixMode(GL_PROJECTION);
    //glGetError(); // not in qt gl
}
void QtOpenGLViewer::endTransformed(GLfloat dimX, GLfloat dimY, GLfloat dimZ, GLfloat locX, GLfloat locY, GLfloat locZ, GLfloat rotX, GLfloat rotY, GLfloat rotZ) {
    glMatrixMode(GL_MODELVIEW);
    glScalef(1.0f/dimX, 1.0f/dimY, 1.0f/dimZ);
    glTranslatef(-locX, -locY, -locZ);
    glRotatef(rotZ, 0.0f, 0.0f, -1.0f);
    glRotatef(rotY, 0.0f, -1.0f, 0.0f);
    glRotatef(rotX, -1.0f, 0.0f, 0.0f);
    glMatrixMode(GL_PROJECTION);
    //glGetError(); // not in qt gl
}
*/