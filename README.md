# AI Media Image

Generate images from a text prompt with any image-capable AI provider and save
them straight into the Backdrop file library.

## What it does

- Adds a **Generate AI Image** action to the file listing at
  `admin/content/files` (form at `admin/content/files/ai-image`), and a
  **Generate AI image** tab at `file/add/ai-image`.
- Lets the user write a prompt and choose the model, a shape and an optional
  filename. When `ai_alt` is enabled, alt text can be generated for the new
  image and stored in the file's alt field.
- Offers every model any enabled provider reports as image-capable (OpenAI,
  Azure, Google Gemini, OpenRouter, ...), defaulting to the site's default
  image model. Nothing is hardcoded to one provider.
- Saves the result as a permanent managed file under
  `{default scheme}://ai-media-images/` (public, S3, ...) and logs the
  generation to watchdog.

## Provider-neutral options

Image APIs disagree on dimensions: OpenAI accepts a few fixed pixel sizes per
model, while Gemini and OpenRouter take aspect ratios. The form therefore
offers shapes (1:1, 3:2, 2:3, 16:9, 9:16) and each provider adapter maps the
shape to the closest size or ratio its API supports, using `AIImageHelper` in
the `ai` module. Provider-specific options such as DALL-E 3's quality and style
are not offered.

Responses are read with `AIImageHelper::extract()`, which handles base64,
remote HTTPS URLs and `data:` URLs, and only accepts real JPEG, PNG or WebP
data. The file extension comes from the detected image type; a custom filename
only supplies the base name.

## Permissions

- **Generate AI Media Images** (`generate ai media images`) — required for the
  form and the action link.

## Installation

- Install this module using the official [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).
- Requires the `ai` and `file` modules, and at least one provider with an
  image generation model configured at `admin/config/ai/settings`.
- Grant the **Generate AI Media Images** permission to the roles that should
  use it.

## Issues

Bugs and feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/ai_media_image/issues).

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).

- Developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
