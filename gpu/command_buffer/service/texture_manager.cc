// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_manager.h"
#include "base/bits.h"

namespace gpu {
namespace gles2 {

static GLsizei ComputeMipMapCount(
    GLsizei width, GLsizei height, GLsizei depth) {
  return 1 + base::bits::Log2Floor(std::max(std::max(width, height), depth));
}

static size_t GLTargetToFaceIndex(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return 0;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
      return 0;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      return 1;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      return 2;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      return 3;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      return 4;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return 5;
    default:
      NOTREACHED();
      return 0;
  }
}

static size_t FaceIndexToGLTarget(size_t index) {
  switch (index) {
    case 0:
      return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    case 1:
      return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
    case 2:
      return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
    case 3:
      return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
    case 4:
      return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
    case 5:
      return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
    default:
      NOTREACHED();
      return 0;
  }
}

void TextureManager::TextureInfo::MarkMipmapsGenerated() {
  DCHECK(CanGenerateMipmaps());
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    const TextureInfo::LevelInfo& info1 = level_infos_[ii][0];
    GLsizei width = info1.width;
    GLsizei height = info1.height;
    GLsizei depth = info1.depth;
    int num_mips = ComputeMipMapCount(width, height, depth);
    for (int level = 1; level < num_mips; ++level) {
      width = std::max(1, width >> 1);
      height = std::max(1, height >> 1);
      depth = std::max(1, depth >> 1);
      SetLevelInfo(target_ == GL_TEXTURE_2D ? GL_TEXTURE_2D :
                                              FaceIndexToGLTarget(ii),
                   level,
                   info1.internal_format,
                   width,
                   height,
                   depth,
                   info1.border,
                   info1.format,
                   info1.type);
    }
  }
}

bool TextureManager::TextureInfo::CanGenerateMipmaps() const {
  if (npot()) {
    return false;
  }
  const TextureInfo::LevelInfo& first = level_infos_[0][0];
  // TODO(gman): Check internal_format, format and type.
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    const LevelInfo& info = level_infos_[ii][0];
    if ((!info.valid) ||
        (info.width != first.width) ||
        (info.height != first.height) ||
        (info.depth != 1) ||
        (info.format != first.format) ||
        (info.internal_format != first.internal_format) ||
        (info.type != first.type)) {
        return false;
    }
  }
  return true;
}

void TextureManager::TextureInfo::SetLevelInfo(
    GLenum target,
    GLint level,
    GLint internal_format,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type) {
  DCHECK_GE(level, 0);
  DCHECK_LT(static_cast<size_t>(GLTargetToFaceIndex(target)),
            level_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            level_infos_[GLTargetToFaceIndex(target)].size());
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);
  DCHECK_GE(depth, 0);
  TextureInfo::LevelInfo& info =
      level_infos_[GLTargetToFaceIndex(target)][level];
  info.valid = true;
  info.internal_format = internal_format;
  info.width = width;
  info.height = height;
  info.depth = depth;
  info.border = border;
  info.format = format;
  info.type = type;
  max_level_set_ = std::max(max_level_set_, level);
  Update();
}

void TextureManager::TextureInfo::Update() {
  // Update npot status.
  npot_ = false;
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    const TextureInfo::LevelInfo& info = level_infos_[ii][0];
    if (((info.width & (info.width - 1)) != 0) ||
        ((info.height & (info.height - 1)) != 0) ||
        ((info.depth & (info.depth - 1)) != 0)) {
      npot_ = true;
      break;
    }
  }

  // Update texture_complete status.
  const TextureInfo::LevelInfo& first_face = level_infos_[0][0];
  texture_complete_ =
      (max_level_set_ == ComputeMipMapCount(first_face.width,
                                            first_face.height,
                                            first_face.depth) - 1) &&
      max_level_set_ >= 0;
  cube_complete_ = (level_infos_.size() == 6) &&
                   (first_face.width == first_face.height);
  for (size_t ii = 0;
       ii < level_infos_.size() && cube_complete_ && texture_complete_;
       ++ii) {
    const TextureInfo::LevelInfo& level0 = level_infos_[ii][0];
    if (!level0.valid ||
        level0.width != first_face.width ||
        level0.height != first_face.height ||
        level0.depth != 1 ||
        level0.internal_format != first_face.internal_format ||
        level0.format != first_face.format ||
        level0.type != first_face.type) {
      cube_complete_ = false;
    }
    // Get level0 dimensions
    GLsizei width = level0.width;
    GLsizei height = level0.height;
    GLsizei depth = level0.depth;
    for (GLint jj = 0; jj <= max_level_set_; ++jj) {
      // compute required size for mip.
      width = std::max(1, width >> 1);
      height = std::max(1, height >> 1);
      depth = std::max(1, depth >> 1);
      const TextureInfo::LevelInfo& info = level_infos_[ii][jj];
      if (!info.valid ||
          info.width != width ||
          info.height != height ||
          info.depth != depth ||
          info.internal_format != level0.internal_format ||
          info.format != level0.format ||
          info.type != level0.type) {
        texture_complete_ = false;
        break;
      }
    }
  }
}

TextureManager::TextureManager(
    GLint max_texture_size, GLint max_cube_map_texture_size)
    : max_texture_size_(max_texture_size),
      max_cube_map_texture_size_(max_cube_map_texture_size),
      max_levels_(ComputeMipMapCount(max_texture_size,
                                     max_texture_size,
                                     max_texture_size)),
      max_cube_map_levels_(ComputeMipMapCount(max_cube_map_texture_size,
                                              max_cube_map_texture_size,
                                              max_cube_map_texture_size)) {
}

void TextureManager::CreateTextureInfo(GLuint texture) {
  TextureInfo* info = new TextureInfo(texture);
  std::pair<TextureInfoMap::iterator, bool> result =
      texture_infos_.insert(
          std::make_pair(texture, linked_ptr<TextureInfo>(info)));
  DCHECK(result.second);
}

TextureManager::TextureInfo* TextureManager::GetTextureInfo(
    GLuint texture) {
  TextureInfoMap::iterator it = texture_infos_.find(texture);
  return it != texture_infos_.end() ? &(*it->second) : NULL;
}

void TextureManager::RemoveTextureInfo(GLuint texture_id) {
  texture_infos_.erase(texture_id);
}

}  // namespace gles2
}  // namespace gpu


