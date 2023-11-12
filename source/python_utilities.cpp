
#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include <jxl/thread_parallel_runner.h>
#include <jxl/thread_parallel_runner_cxx.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <sstream>
#include <string>
#include <vector>

namespace mft::python {
namespace py = pybind11;

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

bool EncodeJxlOneshot(const uint32_t xsize, const uint32_t ysize,
                      const uint32_t channels, const void* pixel_data,

                      std::vector<uint8_t>& compressed) {
  auto enc = JxlEncoderMake(nullptr);
  auto runner = JxlThreadParallelRunnerMake(
      nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
  if (JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner(enc.get(),
                                                     JxlThreadParallelRunner,
                                                     runner.get())) {
    fprintf(stderr, "JxlEncoderSetParallelRunner failed\n");
    return false;
  }

  JxlPixelFormat pixel_format = {channels, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN,
                                 0};

  JxlBasicInfo basic_info;
  JxlEncoderInitBasicInfo(&basic_info);
  basic_info.xsize = xsize;
  basic_info.ysize = ysize;
  basic_info.bits_per_sample = 32;
  basic_info.exponent_bits_per_sample = 8;
  basic_info.uses_original_profile = JXL_FALSE;
  basic_info.num_color_channels = channels;
  if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc.get(), &basic_info)) {
    fprintf(stderr, "JxlEncoderSetBasicInfo failed\n");
    return false;
  }

  JxlColorEncoding color_encoding = {};
  JxlColorEncodingSetToLinearSRGB(&color_encoding, channels < 3);
  if (JXL_ENC_SUCCESS !=
      JxlEncoderSetColorEncoding(enc.get(), &color_encoding)) {
    fprintf(stderr, "JxlEncoderSetColorEncoding failed\n");
    return false;
  }

  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);

  if (JXL_ENC_SUCCESS !=
      JxlEncoderAddImageFrame(frame_settings, &pixel_format, pixel_data,
                              sizeof(float) * xsize * ysize * channels)) {
    fprintf(stderr, "JxlEncoderAddImageFrame failed\n");
    return false;
  }
  JxlEncoderCloseInput(enc.get());

  compressed.resize(64);
  uint8_t* next_out = compressed.data();
  size_t avail_out = compressed.size() - (next_out - compressed.data());
  JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
  while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
    process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
    if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = next_out - compressed.data();
      compressed.resize(compressed.size() * 2);
      next_out = compressed.data() + offset;
      avail_out = compressed.size() - offset;
    }
  }
  compressed.resize(next_out - compressed.data());
  if (JXL_ENC_SUCCESS != process_result) {
    fprintf(stderr, "JxlEncoderProcessOutput failed\n");
    return false;
  }

  return true;
}

bool WriteFile(const std::vector<uint8_t>& bytes, const std::string& path) {
  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    fprintf(stderr, "Could not open %s for writing\n", path.c_str());
    return false;
  }
  if (fwrite(bytes.data(), sizeof(uint8_t), bytes.size(), file) !=
      bytes.size()) {
    fprintf(stderr, "Could not write bytes to %s\n", path.c_str());
    fclose(file);
    return false;
  }
  if (fclose(file) != 0) {
    fprintf(stderr, "Could not close %s\n", path.c_str());
    return false;
  }
  return true;
}

bool SaveJxl(const uint32_t xsize, const uint32_t ysize,
             const uint32_t channels,
             const py::array_t<float, py::array::c_style |
                                          py::array::forcecast>& pixel_data,
             const std::string& path) {
  auto pixel_data_raw = pixel_data.unchecked<>();

  if (pixel_data_raw.size() != xsize * ysize * channels) {
    return false;
  }

  std::vector<uint8_t> compressed;

  if (!EncodeJxlOneshot(xsize, ysize, channels,
                        static_cast<const void*>(pixel_data_raw.data(0, 0, 0)),
                        compressed)) {
    return false;
  }

  if (!WriteFile(compressed, path)) {
    return false;
  }

  return true;
}

PYBIND11_MODULE(mft_tools, m) {
  m.doc() = R"pbdoc(
            Pybind11 example plugin
            -----------------------

            .. currentmodule:: mft_tools

            .. autosummary::
            :toctree: _generate

            save_jxl
        )pbdoc";

  m.def("save_jxl", &SaveJxl, R"pbdoc(
            Save np array to jxl

            saves a an np array as a jxl image
        )pbdoc");

#ifdef VERSION_INFO
  m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
  m.attr("__version__") = "dev";
#endif
}

}  // namespace mft::python