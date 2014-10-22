#pragma once

struct Camera {
    Camera();
    ~Camera();

    void SetViewport(int x, int y, int width, int height);
    void SetClippingPlanes(float zNear, float zFar);
    void SetFOV(float fovRadians);
    void Update(float dt);

    Point3 Position() { return position; }
    Vector3 Forward() { return forward; }

    Matrix4 &ViewMatrix() { return view; }
    Matrix4 &ProjectionMatrix() { return projection; }
    Matrix4 &ViewProjetionMatrix() { return viewProjection; }

    void CaptureKeyboardState();
    void CaptureMouseState();

    int x;
    int y;
    int width;
    int height;

    float fov;
    float nearPlane;
    float farPlane;
    float aspect;

    Point3 position;

    Vector3 velocity;

    Vector3 forward;
    Vector3 up;
    Vector3 right;

    float elevation;

    Quat orientation;

    Matrix4 view;
    Matrix4 projection;
    Matrix4 viewProjection;
};
