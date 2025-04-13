#ifndef HUMANUS_IMAGE_LOADER_H
#define HUMANUS_IMAGE_LOADER_H

#include "httplib.h"

namespace humanus {

struct ImageLoader : BaseTool {
    inline static const std::string name_ = "image_loader";
    inline static const std::string description_ = "Load an image from URL. Returns the image as a base64 encoded string in the format 'data:<mime_type>;base64,<base64_image_data>'.";
    inline static const json parameters_ = json::parse(R"json({
        "type": "object",
        "properties": {
            "url": {
                "type": "string",
                "description": "The URL of the image to load. Supports HTTP/HTTPS URLs and absolute local file paths. If the URL is a local file path, it must start with file://"
            }
        },
        "required": ["url"]
    })json");

    inline static const std::map<std::string, std::string> mime_type_map = {
        {".bmp",  "bmp"},   {".dib", "bmp"},
        {".icns", "icns"},
        {".ico",  "x-icon"},
        {".jfif", "jpeg"},  {".jpe", "jpeg"},  {".jpeg", "jpeg"},  {".jpg", "jpeg"},
        {".j2c",  "jp2"},   {".j2k", "jp2"},   {".jp2",  "jp2"},   {".jpc", "jp2"}, {".jpf", "jp2"},  {".jpx",  "jp2"},
        {".apng", "png"},   {".png", "png"},
        {".bw",   "sgi"},   {".rgb", "sgi"},   {".rgba", "sgi"},   {".sgi", "sgi"}, {".tif", "tiff"}, {".tiff", "tiff"}, {".webp", "webp"},
        {".gif",  "gif"}
    };

    ImageLoader() : BaseTool(name_, description_, parameters_) {}

    std::string get_mime_type(const std::string& path) {
        std::string extension = path.substr(path.find_last_of(".") + 1);
        if (mime_type_map.find(extension) != mime_type_map.end()) {
            return "image/" + mime_type_map.at(extension);
        }
        return "image/png";
    }
    
    ToolResult execute(const json& args) override {
        if (!args.contains("url")) {
            return ToolError("`url` is required");
        }

        std::string binary_content;
        std::string url = args["url"];
        if (url.find("http") == 0) {
            std::string base_url, endpoint;
            size_t pos = url.find("://");
            if (pos == std::string::npos) {
                return ToolError("Invalid URL");
            }
            pos = url.find("/", pos + 3);
            if (pos == std::string::npos) {
                return ToolError("Invalid URL");
            }
            base_url = url.substr(0, pos);
            endpoint = url.substr(pos);
            httplib::Client client(base_url);
            auto res = client.Get(endpoint.c_str());
            if (res->status != 200) {
                return ToolError("Failed to load image from URL");
            }
            binary_content = res->body;
        } else if (url.find("file://") == 0) {
            std::ifstream file(url.substr(7));
            if (!file.is_open()) {
                return ToolError("Invalid file path");
            }
            binary_content = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        } else {
            return ToolError("Invalid URL");
        }
        
        std::string base64_image = httplib::detail::base64_encode(binary_content);
        std::string mime_type = get_mime_type(url);
        std::string image_data = "data:" + mime_type + ";base64," + base64_image;

        json result = {{
            {"type", "image_url"},
            {"image_url", {
                {"url", image_data}
            }}
        }};

        return ToolResult(result);
    }
};

} // namespace humanus

#endif
