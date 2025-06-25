

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <sstream>
#include <string>
#include <vector>

#include "io.h"
#include "jxl.h"

namespace mft {
namespace py = pybind11;

bool save_jxl(const uint32_t xsize, const uint32_t ysize,
              const uint32_t channels,
              const py::array_t<float, py::array::c_style |
                                           py::array::forcecast>& pixel_data,
              const std::string& path) {
  auto pixel_data_raw = pixel_data.unchecked<>();

  if (pixel_data_raw.size() != xsize * ysize * channels) {
    return false;
  }

  std::vector<char> compressed;

  if (!jxl::encode_oneshot(
          xsize, ysize, channels,
          static_cast<const void*>(pixel_data_raw.data(0, 0, 0)), compressed)) {
    return false;
  }

  if (!write_binary(path, compressed)) {
    return false;
  }

  return true;
}

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

PYBIND11_MODULE(mftools, m) {
  m.doc() = R"pbdoc(
            Pybind11 example plugin
            -----------------------

            .. currentmodule:: mftools

            .. autosummary::
            :toctree: _generate

            save_jxl
        )pbdoc";

  m.def("save_jxl", &save_jxl, R"pbdoc(
            Save np array to jxl

            saves a an np array as a jxl image
        )pbdoc");

#ifdef VERSION_INFO
  m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
  m.attr("__version__") = "dev";
#endif
}

}  // namespace mft