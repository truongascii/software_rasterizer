#include <SDL/SDL.h>

#include <vectormath/SSE/cpp/vectormath_aos.h>

#include <IL/IL.h>
#include <IL/ILU.h>
#include <IL/ILUT.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assimp/Importer.hpp>
#include <assimp/Scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

#include <immintrin.h>

typedef Vectormath::Aos::Point3 Point3;
typedef Vectormath::Aos::Vector3 Vector3;
typedef Vectormath::Aos::Vector4 Vector4;
typedef Vectormath::Aos::Matrix3 Matrix3;
typedef Vectormath::Aos::Matrix4 Matrix4;
typedef Vectormath::Aos::Transform3 Transform3;
typedef Vectormath::Aos::Quat Quat;

#include <algorithm>
#include <thread>
#include <atomic>

#undef min
#undef max
