#include "camera.h"

#define CAMERA_LINEAR_SPEED 0.005f

Camera::Camera()
{
    SetViewport(0, 0, 640, 480);
    SetFOV(0.25f * M_PI);
    SetClippingPlanes(1.0f, 100.0f);

    position = Point3(0.0f, 0.0f, 10.0f);
    velocity = Vector3(0.0f, 0.0f, 0.0f);

    forward = -Vector3::zAxis();
    up = Vector3::yAxis();
    right = Vector3::xAxis();

    elevation = 0.0f;

    orientation = Quat::identity();

    view = Matrix4::identity();
    projection = Matrix4::identity();
}

Camera::~Camera() {}

void Camera::SetViewport(int x, int y, int width, int height)
{
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;

    aspect = width / (float)height;
}

void Camera::SetClippingPlanes(float zNear, float zFar)
{
    nearPlane = zNear;
    farPlane = zFar;
}

void Camera::SetFOV(float fovRadians)
{
    fov = fovRadians;
}

void Camera::Update(float dt)
{
    CaptureMouseState();
    CaptureKeyboardState();

    position += velocity * dt;

    forward = normalize(rotate(orientation, -Vector3::zAxis()));
    up = normalize(rotate(orientation, Vector3::yAxis()));
    right = normalize(rotate(orientation, Vector3::xAxis()));

    view = Matrix4::lookAt(position, position + forward, up);
    projection = Matrix4::perspective(fov, aspect, nearPlane, farPlane);
    viewProjection = projection * view;
}

void Camera::CaptureKeyboardState()
{
    const Uint8 *keys = SDL_GetKeyboardState(nullptr);

    if (keys[SDL_SCANCODE_W]) {
        velocity = forward * CAMERA_LINEAR_SPEED;
    } else if (keys[SDL_SCANCODE_S]) {
        velocity = -forward * CAMERA_LINEAR_SPEED;
    } else {
        velocity = Vector3(0.0f, 0.0f, 0.0f);
    }

    if (keys[SDL_SCANCODE_A]) {
        velocity -= right * CAMERA_LINEAR_SPEED;
    } else if (keys[SDL_SCANCODE_D]) {
        velocity += right * CAMERA_LINEAR_SPEED;
    }

    if (keys[SDL_SCANCODE_SPACE]) {
        velocity -= up * CAMERA_LINEAR_SPEED;
    }
}

void Camera::CaptureMouseState()
{
    float maxElevation = 0.49f * (float)M_PI;

    int dx, dy;

    SDL_GetRelativeMouseState(&dx, &dy);

    float rotationX = dy / (float)height;
    float rotationY = dx / (float)width;

    elevation += rotationX;

    if (elevation > maxElevation) {
        rotationX = maxElevation - (elevation - rotationX);
        elevation = maxElevation;
    }

    if (elevation < -maxElevation) {
        rotationX = -maxElevation - (elevation - rotationX);
        elevation = -maxElevation;
    }

    if (rotationY != 0.0f) {
        orientation = Quat::rotation(-rotationY, Vector3::yAxis()) * orientation;
    }

    if (rotationX != 0.0f) {
        orientation = orientation * Quat::rotation(rotationX, Vector3::xAxis());
    }

    orientation = normalize(orientation);
}
