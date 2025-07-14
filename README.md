# qMSDFgen

A Qt-based GUI application for generating Multi-channel Signed Distance Fields (MSDF) from vector shapes and SVG files.

## Overview

qMSDFgen is a user-friendly graphical interface built on top of the [msdfgen](https://github.com/Chlumsky/msdfgen) library. It provides an easy way to convert SVG files into high-quality distance field textures that can be used in real-time graphics applications.

Unlike conventional monochrome distance fields, multi-channel distance fields utilize all three color channels (RGB) to reproduce sharp corners and fine details with superior quality, making them ideal for text rendering and vector graphics in games and real-time applications.

## Features

- **Simple GUI Interface**: Easy-to-use Qt-based interface
- **SVG to PNG Conversion**: Convert SVG vector files to PNG distance field textures
- **Real-time Preview**: View the generated distance field in the application
- **High Quality Output**: Leverages MSDF technology for superior edge reproduction
- **Cross-platform**: Built with Qt for Windows, Linux, and macOS support

## Requirements

- Qt 6.9.0 or later
- FreeType library
- libpng
- TinyXML2

## Building

This project uses qmake as the build system. To build the application:

1. Ensure you have Qt 6.9.0 or later installed
2. Install the required dependencies:
   - FreeType
   - libpng
   - TinyXML2

3. Build the project:
   ```bash
   qmake qMSDFgen.pro
   make
   ```

4. The executable will be generated in the `_bin/` directory

## Usage

1. Launch the qMSDFgen application
2. Click "Browse input" to select an SVG file
3. Click "Browse output" to specify the output PNG file path
4. Click "Save" to generate the distance field texture
5. The generated image will be displayed in the preview area

## Technical Details

### MSDF Technology

Multi-channel Signed Distance Fields (MSDF) represent a significant improvement over traditional signed distance fields:

- **Better Edge Quality**: Sharp corners and fine details are preserved more accurately
- **Efficient Storage**: Uses RGB channels to encode distance information
- **Real-time Rendering**: Optimized for GPU-based rendering in real-time applications

### Project Structure

```
qMSDFgen/
├── src/                    # Qt GUI application source code
│   ├── main.cpp           # Application entry point
│   ├── MainWindow.cpp     # Main window implementation
│   ├── MainWindow.h       # Main window header
│   ├── MainWindow.ui      # UI layout file
│   ├── MyImageView.cpp    # Custom image display widget
│   └── MyImageView.h      # Image view header
├── msdfgen/               # msdfgen library source code
├── config/                # Configuration files
├── _bin/                  # Built executables
├── build/                 # Build artifacts
└── example/               # Example files
```

## License

This project incorporates the msdfgen library. Please refer to the original msdfgen license terms in the `msdfgen/LICENSE.txt` file.

## Acknowledgments

- [msdfgen](https://github.com/Chlumsky/msdfgen) by Viktor Chlumský for the core MSDF generation algorithm
- Qt framework for the GUI components
- FreeType, libpng, and TinyXML2 for additional functionality

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Related Projects

- [msdfgen](https://github.com/Chlumsky/msdfgen) - The core MSDF generation library
- [msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen) - For generating entire font atlases
