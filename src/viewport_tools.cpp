#include "viewport_tools.h"

#ifdef MEOW_GODOT_MCP_GODOT_ENABLED

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

nlohmann::json capture_viewport(const std::string& viewport_type,
                                 int width, int height) {
    // Step 1: Get SubViewport based on type
    SubViewport* viewport = nullptr;
    if (viewport_type == "2d") {
        viewport = EditorInterface::get_singleton()->get_editor_viewport_2d();
    } else if (viewport_type == "3d") {
        viewport = EditorInterface::get_singleton()->get_editor_viewport_3d(0);
    } else {
        return {{"error", "Invalid viewport_type: " + viewport_type + ". Valid values: 2d, 3d"}};
    }

    if (!viewport) {
        return {{"error", "Viewport not available for type: " + viewport_type}};
    }

    // Step 2: Get texture from viewport
    Ref<ViewportTexture> tex = viewport->get_texture();
    if (!tex.is_valid()) {
        return {{"error", "Viewport texture not available"}};
    }

    // Step 3: Download GPU texture to CPU Image
    Ref<Image> image = tex->get_image();
    if (!image.is_valid() || image->is_empty()) {
        return {{"error", "Viewport image is empty (viewport may not be visible)"}};
    }

    // Record original dimensions for metadata
    int orig_width = image->get_width();
    int orig_height = image->get_height();

    // Step 4: Optional resize
    if (width > 0 && height > 0) {
        image->resize(width, height, Image::INTERPOLATE_LANCZOS);
    } else if (width > 0) {
        // Scale proportionally from width
        double scale = static_cast<double>(width) / orig_width;
        int scaled_height = static_cast<int>(orig_height * scale);
        image->resize(width, scaled_height, Image::INTERPOLATE_LANCZOS);
    } else if (height > 0) {
        // Scale proportionally from height
        double scale = static_cast<double>(height) / orig_height;
        int scaled_width = static_cast<int>(orig_width * scale);
        image->resize(scaled_width, height, Image::INTERPOLATE_LANCZOS);
    }

    // Step 5: Encode as PNG bytes
    PackedByteArray png_data = image->save_png_to_buffer();
    if (png_data.size() == 0) {
        return {{"error", "PNG encoding failed"}};
    }

    // Step 6: Base64 encode using Godot's Marshalls singleton
    String base64_godot = Marshalls::get_singleton()->raw_to_base64(png_data);
    std::string base64_str(base64_godot.utf8().get_data());

    // Step 7: Build result with metadata
    nlohmann::json metadata = {
        {"viewport_type", viewport_type},
        {"width", image->get_width()},
        {"height", image->get_height()},
        {"original_width", orig_width},
        {"original_height", orig_height}
    };

    return {
        {"data", base64_str},
        {"mimeType", "image/png"},
        {"metadata", metadata}
    };
}

#endif // MEOW_GODOT_MCP_GODOT_ENABLED
