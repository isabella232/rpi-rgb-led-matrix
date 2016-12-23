// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Small example how to use the library.
// For more examples, look at demo-main.cc
//
// This code is public domain
// (but note, that the led-matrix library this depends on is GPL v2)

#include "led-matrix.h"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <signal.h>
#include <string>
#include <unordered_map>

using rgb_matrix::GPIO;
using rgb_matrix::RGBMatrix;
using rgb_matrix::Canvas;

#define PANEL1 0
#define PANEL2 33
#define PANEL3 65
#define PANEL4 97

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
  interrupt_received = true;
}

struct Pixel {
  Pixel() : red(0), green(0), blue(0){}
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct Image {
  Image() : width(-1), height(-1), image(NULL) {}
  ~Image() { Delete(); }
  void Delete() { delete [] image; Reset(); }
  void Reset() { image = NULL; width = -1; height = -1; }
  inline bool IsValid() { return image && height > 0 && width > 0; }
  const Pixel &getPixel(int x, int y) {
    static Pixel black;
    if (x < 0 || x >= width || y < 0 || y >= height) return black;
    return image[x + width * y];
  }

  int width;
  int height;
  Pixel *image;
};

char *ReadLine(FILE *f, char *buffer, size_t len) {
  char *result;
  do {
    result = fgets(buffer, len, f);
  } while (result != NULL && result[0] == '#');
  return result;
}

Image * LoadPPM(const char *filename) {
  FILE *f = fopen(filename, "r");
  // check if file exists
  if (f == NULL && access(filename, F_OK) == -1) {
    fprintf(stderr, "File \"%s\" doesn't exist\n", filename);
  }
  char header_buf[256];
  const char *line = ReadLine(f, header_buf, sizeof(header_buf));
#define EXIT_WITH_MSG(m) { fprintf(stderr, "%s: %s |%s", filename, m, line); \
    fclose(f); return NULL; }
  if (sscanf(line, "P6 ") == EOF)
    EXIT_WITH_MSG("Can only handle P6 as PPM type.");
  line = ReadLine(f, header_buf, sizeof(header_buf));
  int new_width, new_height;
  if (!line || sscanf(line, "%d %d ", &new_width, &new_height) != 2)
    EXIT_WITH_MSG("Width/height expected");
  int value;
  line = ReadLine(f, header_buf, sizeof(header_buf));
  if (!line || sscanf(line, "%d ", &value) != 1 || value != 255)
    EXIT_WITH_MSG("Only 255 for maxval allowed.");
  const size_t pixel_count = new_width * new_height;
  Pixel *new_image = new Pixel [ pixel_count ];
  if (fread(new_image, sizeof(Pixel), pixel_count, f) != pixel_count) {
    line = "";
    EXIT_WITH_MSG("Not enough pixels read.");
  }
#undef EXIT_WITH_MSG
  fclose(f);
  fprintf(stderr, "Read image '%s' with %dx%d\n", filename, new_width, new_height);
  Image *image = new Image;
  image->image = new_image;
  image->width = new_width;
  image->height = new_height;

  image->image = new_image;
  return image;
}

static void DrawOnCanvas(Canvas *canvas, int start_position, Image * image) {
  std::cout << "Drawing on panel " << start_position << std::endl;
  for (int x = 0; x < image->width; ++x) {
    for (int y = 0; y < image->height; ++y) {
      const Pixel &p = image->getPixel(x, y);
      canvas->SetPixel(x + start_position, y, p.red, p.green, p.blue);
    }
  }
}

static void BlankScreen(Canvas *canvas, int start_position) {
  std::cout << "Blanking panel " << start_position << std::endl;
  for (int x = 0; x < 32; ++x) {
    for (int y = 0; y < 16; ++y) {
      canvas->SetPixel(x + start_position, y, 0, 0, 0);
    }
  }
}

int main(int argc, char *argv[]) {
  RGBMatrix::Options defaults;
  defaults.hardware_mapping = "regular";  // or e.g. "adafruit-hat"
  defaults.rows = 16;
  defaults.chain_length = 4;
  defaults.parallel = 1;
  defaults.show_refresh_rate = false;
  defaults.brightness = 60;
  Canvas *canvas = rgb_matrix::CreateMatrixFromFlags(&argc, &argv, &defaults);
  if (canvas == NULL)
    return 1;

  // It is always good to set up a signal handler to cleanly exit when we
  // receive a CTRL-C for instance. The DrawOnCanvas() routine is looking
  // for that.
  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);

  std::unordered_map<std::string,Image *> images = {
    { "eve", LoadPPM("/home/pi/tb/rpi-rgb-led-matrix/examples-api-use/images/eve.ppm") },
    { "optimus", LoadPPM("/home/pi/tb/rpi-rgb-led-matrix/examples-api-use/images/optimus.ppm") },
    { "walle", LoadPPM("/home/pi/tb/rpi-rgb-led-matrix/examples-api-use/images/walle.ppm") },
    { "bender", LoadPPM("/home/pi/tb/rpi-rgb-led-matrix/examples-api-use/images/bender.ppm") }
  };
  std::unordered_map<std::string,int> panels = {
    { "eve", PANEL1 },
    { "optimus", PANEL2 },
    { "walle", PANEL3 },
    { "bender", PANEL4 }
  };
  std::unordered_map<std::string,int> availability = {
    { "eve", NULL },
    { "optimus", NULL },
    { "walle", NULL },
    { "bender", NULL }
  };

  std::ifstream infile;
  std::string room;
  int current_availability;
  std::string line;

  while(true) {
    infile.open("/home/pi/tb/rpi-rgb-led-matrix/examples-api-use/availability");
    if(infile.is_open()) {
      while(infile >> room >> current_availability)
      {
        std::cout << room << current_availability << std::endl;
        if(current_availability != availability[room]) {
          if(current_availability == 1) {
            DrawOnCanvas(canvas, panels[room], images[room]);
          } else {
            BlankScreen(canvas, panels[room]);
          }
        }
        availability[room] = current_availability;
      }
      infile.close();
    }
    sleep(30);
  }

  // Animation finished. Shut down the RGB matrix.
  canvas->Clear();
  delete canvas;

  return 0;
}
