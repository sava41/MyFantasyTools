#include "jxl.h"

#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include <jxl/thread_parallel_runner.h>
#include <jxl/thread_parallel_runner_cxx.h>

#include "io.h"

namespace mft {

bool EncodeJxlOneshot(const uint32_t xsize, const uint32_t ysize,
                      const uint32_t channels, const void* pixelData,
                      std::vector<char>& compressed) {
  auto enc = JxlEncoderMake(nullptr);
  auto runner = JxlThreadParallelRunnerMake(
      nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());

  if (JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner(enc.get(),
                                                     JxlThreadParallelRunner,
                                                     runner.get())) {
    fprintf(stderr, "JxlEncoderSetParallelRunner failed\n");
    return false;
  }

  JxlPixelFormat pixelFormat = {channels, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0};

  JxlBasicInfo basicInfo;
  JxlEncoderInitBasicInfo(&basicInfo);
  basicInfo.xsize = xsize;
  basicInfo.ysize = ysize;
  basicInfo.bits_per_sample = 32;
  basicInfo.exponent_bits_per_sample = 8;
  basicInfo.uses_original_profile = JXL_FALSE;
  basicInfo.num_color_channels = channels;
  if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc.get(), &basicInfo)) {
    fprintf(stderr, "JxlEncoderSetBasicInfo failed\n");
    return false;
  }

  JxlColorEncoding colorEncoding = {};
  JxlColorEncodingSetToLinearSRGB(&colorEncoding, channels < 3);
  if (JXL_ENC_SUCCESS !=
      JxlEncoderSetColorEncoding(enc.get(), &colorEncoding)) {
    fprintf(stderr, "JxlEncoderSetColorEncoding failed\n");
    return false;
  }

  JxlEncoderFrameSettings* frameSettings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);

  // JxlEncoderSetFrameLossless(frameSettings, JXL_FALSE);

  if (JXL_ENC_SUCCESS !=
      JxlEncoderAddImageFrame(frameSettings, &pixelFormat, pixelData,
                              sizeof(float) * xsize * ysize * channels)) {
    fprintf(stderr, "JxlEncoderAddImageFrame failed\n");
    return false;
  }
  JxlEncoderCloseInput(enc.get());

  compressed.resize(16384);
  uint8_t* nextOut = reinterpret_cast<uint8_t*>(compressed.data());
  size_t availOut = static_cast<size_t>(compressed.size());

  JxlEncoderStatus result = JXL_ENC_NEED_MORE_OUTPUT;
  while (result == JXL_ENC_NEED_MORE_OUTPUT) {
    result = JxlEncoderProcessOutput(enc.get(), &nextOut, &availOut);

    if (result == JXL_ENC_NEED_MORE_OUTPUT) {
      size_t offset = nextOut - reinterpret_cast<uint8_t*>(compressed.data());
      compressed.resize(compressed.size() * 2);
      nextOut = reinterpret_cast<uint8_t*>(compressed.data()) + offset;
      availOut = static_cast<size_t>(compressed.size()) - offset;
    }
  }
  compressed.resize(nextOut - reinterpret_cast<uint8_t*>(compressed.data()));

  if (JXL_ENC_SUCCESS != result) {
    fprintf(stderr, "JxlEncoderProcessOutput failed\n");
    return false;
  }

  return true;
}
}  // namespace mft