// Draws one room of a compiled map to a PPM, without opening a window
//
// It reads the same .map the game reads, stands the camera where the room says,
// and shades every triangle the way simple_shader.frag does, so a layout change
// can be looked at from WSL without a swapchain
//
// Build and run from the repo root:
//   g++ -std=c++17 -O2 -Iengine/include -Iengine/libs -Igame/src \
//       tools/preview.cpp game/src/petscop/map_loader.cpp engine/src/lve_camera.cpp \
//       -o /tmp/preview
//   /tmp/preview maps/petscop.map Yard /tmp/yard.ppm

#include "lve_camera.hpp"
#include "petscop/map_loader.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader/tiny_obj_loader.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

namespace {

const int WIDTH = 1200;
const int HEIGHT = 900;
const glm::vec4 AMBIENT{1.f, 1.f, 1.f, 0.15f};
const float HIDE_THRESHOLD = 0.25f;
const float GHOST_ALPHA = 0.25f;

// How far one width of a picture reaches, matching RoomScene
const float FLOOR_TILE_METRES = 3.f;
const float WALL_TILE_METRES = 1.8f;

// The floor the map is built on, matching build_map.py
const float GROUND_Y = 0.5f;

struct Vertex {
  glm::vec3 position{0.f};
  glm::vec3 normal{0.f};
  glm::vec3 color{1.f};
};

struct Mesh {
  std::vector<Vertex> vertices;  // three in a row make a triangle
};

// Everything the shader needs about one pixel of one triangle
struct Fragment {
  glm::vec3 world{0.f};
  glm::vec3 normal{0.f};
  glm::vec3 color{1.f};
};

// A .tex, held the way the sampler would read it back
struct Picture {
  int width = 0;
  int height = 0;
  std::vector<glm::vec3> pixels;  // already out of sRGB
};

std::map<std::string, Mesh> meshes;
std::map<std::string, Picture> pictures;

// Reads what tools/png2tex.py wrote, or null when there is nothing to read
const Picture* loadPicture(const std::string& name) {
  auto found = pictures.find(name);
  if (found != pictures.end()) return found->second.width ? &found->second : nullptr;

  Picture picture;
  const std::string path = "textures/" + name + ".tex";
  std::ifstream file(path, std::ios::binary);

  char magic[8] = {};
  unsigned int header[2] = {0, 0};
  if (file.is_open()) {
    file.read(magic, sizeof(magic));
    file.read((char*)header, sizeof(header));
  }

  if (!file || std::string(magic, 8) != "LVETEX01") {
    std::cerr << "preview: no picture at " << path << "\n";
    pictures[name] = picture;
    return nullptr;
  }

  picture.width = (int)header[0];
  picture.height = (int)header[1];
  std::vector<unsigned char> bytes((std::size_t)picture.width * picture.height * 4);
  file.read((char*)bytes.data(), (std::streamsize)bytes.size());

  picture.pixels.resize((std::size_t)picture.width * picture.height);
  for (std::size_t i = 0; i < picture.pixels.size(); i++) {
    picture.pixels[i] = glm::vec3(std::pow(bytes[i * 4 + 0] / 255.f, 2.2f),
                                  std::pow(bytes[i * 4 + 1] / 255.f, 2.2f),
                                  std::pow(bytes[i * 4 + 2] / 255.f, 2.2f));
  }

  pictures[name] = picture;
  return &pictures[name];
}

// The mirrored repeat the engine's sampler is set to
float mirror(float value) {
  value = std::fabs(std::fmod(value, 2.f));
  return value > 1.f ? 2.f - value : value;
}

glm::vec3 samplePicture(const Picture& picture, float u, float v) {
  const int x = std::min(picture.width - 1, (int)(mirror(u) * picture.width));
  const int y = std::min(picture.height - 1, (int)(mirror(v) * picture.height));
  return picture.pixels[(std::size_t)y * picture.width + x];
}

// What one surface is painted with, and how the picture sits on it
struct Paint {
  const Picture* picture = nullptr;
  glm::vec2 tile{1.f};
  float ground = GROUND_Y;
};

// The same two axes textured_shader.frag picks, off the way the surface faces
glm::vec2 surfaceAt(const glm::vec3& world, const glm::vec3& normal, float ground) {
  const glm::vec3 facing = glm::abs(normal);
  if (facing.y >= facing.x && facing.y >= facing.z) return glm::vec2(world.x, world.z);

  // -Y is up, so the height above the floor grows as y shrinks
  const float climb = ground - world.y;
  return facing.z >= facing.x ? glm::vec2(world.x, climb) : glm::vec2(world.z, climb);
}

const Mesh* loadMesh(const std::string& preset) {
  auto found = meshes.find(preset);
  if (found != meshes.end()) return &found->second;

  const std::string path = "models/" + preset + ".obj";
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;
  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
    std::cerr << "preview: cannot read " << path << ": " << warn << err << "\n";
    return nullptr;
  }

  Mesh mesh;
  for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
      Vertex vertex;
      if (index.vertex_index >= 0) {
        vertex.position = {attrib.vertices[3 * index.vertex_index + 0],
                           attrib.vertices[3 * index.vertex_index + 1],
                           attrib.vertices[3 * index.vertex_index + 2]};
        vertex.color = {attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2]};
      }
      if (index.normal_index >= 0) {
        vertex.normal = {attrib.normals[3 * index.normal_index + 0],
                         attrib.normals[3 * index.normal_index + 1],
                         attrib.normals[3 * index.normal_index + 2]};
      }
      mesh.vertices.push_back(vertex);
    }
  }
  return &(meshes[preset] = mesh);
}

// The same translate, rotate then scale the game stands a prop with
glm::mat4 placement(const glm::vec3& t, const glm::vec3& r, const glm::vec3& s) {
  glm::mat4 mat = glm::translate(glm::mat4(1.f), t);
  mat = glm::rotate(mat, r.y, glm::vec3(0.f, 1.f, 0.f));
  mat = glm::rotate(mat, r.x, glm::vec3(1.f, 0.f, 0.f));
  mat = glm::rotate(mat, r.z, glm::vec3(0.f, 0.f, 1.f));
  return glm::scale(mat, s);
}

// A wall turned toward the camera goes see-through, exactly as the room does it
float visibilityOf(const petscop::MapObject& object, const glm::vec3& eye, const glm::vec3& look) {
  if (object.face == glm::vec3(0.f)) return 1.f;
  const glm::vec3 forward = look - eye;
  glm::vec2 forwardFlat(forward.x, forward.z);
  glm::vec2 outwardFlat(object.face.x, object.face.z);
  if (glm::length(forwardFlat) < 1e-4f || glm::length(outwardFlat) < 1e-4f) return 1.f;
  const float facing = glm::dot(glm::normalize(outwardFlat), glm::normalize(forwardFlat));
  return facing < HIDE_THRESHOLD ? GHOST_ALPHA : 1.f;
}

glm::vec3 shade(const Fragment& fragment, const petscop::MapRoom& room, const glm::vec3& eye,
                const Paint& paint) {
  glm::vec3 diffuse = glm::vec3(AMBIENT) * AMBIENT.w;
  glm::vec3 specular(0.f);
  const glm::vec3 normal = glm::normalize(fragment.normal);
  const glm::vec3 toView = glm::normalize(eye - fragment.world);

  for (const petscop::MapLight& light : room.lights) {
    glm::vec3 toLight = light.position - fragment.world;
    const float attenuation = 1.f / glm::dot(toLight, toLight);
    toLight = glm::normalize(toLight);
    const glm::vec3 intensity = light.color * light.intensity * attenuation;
    diffuse += intensity * std::max(glm::dot(normal, toLight), 0.f);
    const glm::vec3 half = glm::normalize(toLight + toView);
    specular += intensity * std::pow(std::max(glm::dot(normal, half), 0.f), 32.f);
  }
  // A painted surface takes its colour off the picture, laid out by where it is
  glm::vec3 albedo = fragment.color;
  if (paint.picture) {
    const glm::vec2 surface = surfaceAt(fragment.world, normal, paint.ground);
    albedo *= samplePicture(*paint.picture, surface.x * paint.tile.x, surface.y * paint.tile.y);
    specular *= 0.25f;
  }
  return diffuse * albedo + specular * albedo;
}

struct Target {
  std::vector<glm::vec3> color;
  std::vector<float> depth;

  Target() : color(WIDTH * HEIGHT, glm::vec3(0.01f, 0.01f, 0.02f)), depth(WIDTH * HEIGHT, 1.f) {}
};

// Walks one triangle's pixels, keeping the nearest and blending by alpha
void rasterize(Target& target, const glm::vec4 clip[3], const Vertex vertex[3],
               const petscop::MapRoom& room, const glm::vec3& eye, float alpha, bool writeDepth,
               const Paint& paint) {
  for (int i = 0; i < 3; i++) {
    if (clip[i].w < 1e-4f) return;  // behind the camera, dropped rather than clipped
  }

  glm::vec3 screen[3];
  float invW[3];
  for (int i = 0; i < 3; i++) {
    invW[i] = 1.f / clip[i].w;
    screen[i] = {(clip[i].x * invW[i] * 0.5f + 0.5f) * WIDTH,
                 (clip[i].y * invW[i] * 0.5f + 0.5f) * HEIGHT,
                 clip[i].z * invW[i]};
  }

  const float area = (screen[1].x - screen[0].x) * (screen[2].y - screen[0].y) -
                     (screen[2].x - screen[0].x) * (screen[1].y - screen[0].y);
  if (std::abs(area) < 1e-6f) return;

  int minX = std::max(0, (int)std::floor(std::min({screen[0].x, screen[1].x, screen[2].x})));
  int maxX = std::min(WIDTH - 1, (int)std::ceil(std::max({screen[0].x, screen[1].x, screen[2].x})));
  int minY = std::max(0, (int)std::floor(std::min({screen[0].y, screen[1].y, screen[2].y})));
  int maxY = std::min(HEIGHT - 1, (int)std::ceil(std::max({screen[0].y, screen[1].y, screen[2].y})));

  for (int y = minY; y <= maxY; y++) {
    for (int x = minX; x <= maxX; x++) {
      const float px = x + 0.5f, py = y + 0.5f;
      float w0 = ((screen[1].x - px) * (screen[2].y - py) - (screen[2].x - px) * (screen[1].y - py)) / area;
      float w1 = ((screen[2].x - px) * (screen[0].y - py) - (screen[0].x - px) * (screen[2].y - py)) / area;
      float w2 = 1.f - w0 - w1;
      if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;

      const float depth = w0 * screen[0].z + w1 * screen[1].z + w2 * screen[2].z;
      if (depth < 0.f || depth > 1.f) continue;
      const std::size_t at = y * WIDTH + x;
      if (depth >= target.depth[at]) continue;

      // Perspective correct, otherwise a long wall bends toward the camera
      const float sum = w0 * invW[0] + w1 * invW[1] + w2 * invW[2];
      const float p0 = w0 * invW[0] / sum, p1 = w1 * invW[1] / sum, p2 = w2 * invW[2] / sum;

      Fragment fragment;
      fragment.world = p0 * vertex[0].position + p1 * vertex[1].position + p2 * vertex[2].position;
      fragment.normal = p0 * vertex[0].normal + p1 * vertex[1].normal + p2 * vertex[2].normal;
      fragment.color = p0 * vertex[0].color + p1 * vertex[1].color + p2 * vertex[2].color;
      if (glm::length(fragment.normal) < 1e-5f) continue;

      const glm::vec3 lit = shade(fragment, room, eye, paint);
      target.color[at] = glm::mix(target.color[at], lit, alpha);
      if (writeDepth) target.depth[at] = depth;
    }
  }
}

void drawObject(Target& target, const petscop::MapObject& object, const petscop::MapRoom& room,
                const std::vector<std::string>& presets,
                const glm::mat4& viewProjection, const glm::vec3& eye, float alpha) {
  const Mesh* mesh = loadMesh(presets[object.preset]);
  if (!mesh) return;

  const glm::mat4 model = placement(object.translation, object.rotation, object.scale);
  const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

  Paint paint;
  paint.picture = object.texture.empty() ? nullptr : loadPicture(object.texture);
  if (paint.picture) {
    const bool wall = object.face != glm::vec3(0.f);
    const float metres = wall ? WALL_TILE_METRES : FLOOR_TILE_METRES;
    const float aspect = (float)paint.picture->width / (float)paint.picture->height;
    paint.tile = glm::vec2(1.f, aspect) / metres;
  }

  for (std::size_t i = 0; i + 2 < mesh->vertices.size(); i += 3) {
    Vertex world[3];
    glm::vec4 clip[3];
    for (int c = 0; c < 3; c++) {
      const Vertex& source = mesh->vertices[i + c];
      world[c].position = glm::vec3(model * glm::vec4(source.position, 1.f));
      world[c].normal = normalMatrix * source.normal;
      world[c].color = source.color;
      clip[c] = viewProjection * glm::vec4(world[c].position, 1.f);
    }
    rasterize(target, clip, world, room, eye, alpha, alpha >= 1.f, paint);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "preview <map> <room name> <out.ppm>\n";
    return 2;
  }

  petscop::GameMap map;
  std::string error;
  if (!petscop::loadMap(argv[1], map, error)) {
    std::cerr << "preview: " << error << "\n";
    return 1;
  }

  const petscop::MapRoom* room = nullptr;
  for (const petscop::MapRoom& candidate : map.rooms) {
    if (candidate.name == argv[2]) room = &candidate;
  }
  if (!room) {
    std::cerr << "preview: no room called " << argv[2] << "\n";
    return 1;
  }

  lve::LveCamera camera;
  camera.setViewTarget(room->cameraEye, room->cameraLook);
  camera.setPerspectiveProjection(glm::radians(50.f), float(WIDTH) / float(HEIGHT), 0.1f, 100.f);
  const glm::mat4 viewProjection = camera.getProjection() * camera.getView();


  Target target;
  std::vector<const petscop::MapObject*> ghosts;
  for (const petscop::MapObject& object : room->objects) {
    const float alpha = visibilityOf(object, room->cameraEye, room->cameraLook);
    if (alpha < 1.f) {
      ghosts.push_back(&object);
      continue;
    }
    drawObject(target, object, *room, map.presets, viewProjection, room->cameraEye, 1.f);
  }
  for (const petscop::MapObject* object : ghosts) {
    drawObject(target, *object, *room, map.presets, viewProjection, room->cameraEye, GHOST_ALPHA);
  }

  std::ofstream out(argv[3], std::ios::binary);
  out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
  for (const glm::vec3& pixel : target.color) {
    for (int c = 0; c < 3; c++) {
      const float value = std::pow(std::min(std::max(pixel[c], 0.f), 1.f), 1.f / 2.2f);
      const unsigned char byte = (unsigned char)(value * 255.f + 0.5f);
      out.write((const char*)&byte, 1);
    }
  }
  std::cout << "preview: " << argv[2] << " -> " << argv[3] << "\n";
  return 0;
}
