#include "cio.H"
#include "funcdefs.H"
#include <hobbes/db/file.H>
#include <hobbes/ipc/net.H>
#include <hobbes/util/perf.H>
#include <hobbes/util/str.H>
#include <hobbes/util/time_util.H>

#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "GLFW/glfw3.h"
#include "TH/TH.h"
#include "TH/THFilePrivate.h"

namespace hit {

struct Window {
  GLFWwindow *window;
};

auto createWindow(uint width, uint height, hobbes::array<char> *title)
    -> Window {
  Window w;
  w.window = glfwCreateWindow(width, height,
                              hobbes::makeStdString(title).c_str(), NULL, NULL);
  return w;
}

auto makeContextCurrent(Window w) -> void { glfwMakeContextCurrent(w.window); }

// allow local evaluations to run processes, write files
void writefile(const hobbes::array<char> *fname,
               const hobbes::array<char> *fdata) {
  std::string fn = hobbes::makeStdString(fname);
  std::ofstream f(fn.c_str());

  if (f.is_open()) {
    f << fdata;
    f.close();
  }
}

int openfd(const hobbes::array<char> *fname, int flags) {
  return ::open(makeStdString(fname).c_str(), flags,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
}

void closefd(int fd) { ::close(fd); }

void removefile(const hobbes::array<char> *fname) {
  unlink(hobbes::makeStdString(fname).c_str());
}

const hobbes::array<char> *showTick(long x) {
  return hobbes::makeString(hobbes::describeNanoTime(x));
}

// allow local evaluations to determine terminal behavior
void enableConsoleCmds(bool f);

// spawn sub-processes and support basic I/O
typedef std::pair<int, int> PIO;

const PIO *pexec(const hobbes::array<char> *cmd) {
  PIO *p = hobbes::make<PIO>(0, 0);

  int p2c[2] = {0, 0};
  int c2p[2] = {0, 0};

  if (pipe(p2c) < 0) {
    return p;
  }
  if (pipe(c2p) < 0) {
    return p;
  }

  pid_t cpid = fork();

  if (cpid == -1) {
    return p;
  } else if (cpid == 0) {
    // we're in the child process, so we should just redirect pipes and then
    // execute the requested program
    close(p2c[1]);
    dup2(p2c[0], STDIN_FILENO);
    close(p2c[0]);

    close(c2p[0]);
    dup2(c2p[1], STDOUT_FILENO);
    dup2(c2p[1], STDERR_FILENO);
    close(c2p[1]);

    hobbes::str::seq args =
        hobbes::str::csplit(hobbes::makeStdString(cmd), " ");
    if (args.size() == 0)
      return p;

    std::vector<const char *> argv;
    for (size_t i = 0; i < args.size(); ++i) {
      argv.push_back(args[i].c_str());
    }
    argv.push_back(0);

    execv(args[0].c_str(), (char *const *)&argv[0]);
    exit(0);
  } else {
    close(p2c[0]);
    p2c[0] = 0;
    close(c2p[1]);
    c2p[1] = 0;

    p->first = p2c[1];
    p->second = c2p[0];
  }
  return p;
}

const hobbes::array<char> *fdReadLine(int fd) {
  std::ostringstream line;

  char c;
  while (true) {
    if (read(fd, &c, 1) == 1) {
      if (c == '\n') {
        break;
      } else {
        line << c;
      }
    } else {
      break;
    }
  }

  return hobbes::makeString(line.str());
}

const hobbes::array<char> *runPath() {
  char buf[PATH_MAX];
  ssize_t len = -1;

  if ((len = readlink("/proc/self/exe", buf, sizeof(buf) - 1)) != -1) {
    return hobbes::makeString(
        hobbes::str::rsplit(std::string(buf, len), "/").first);
  } else {
    return hobbes::makeString(std::string("./"));
  }
}

// This leaks memory unless it is freed explicitely
const char *foreignString(hobbes::array<char> *string) {
  auto stdstring = hobbes::makeStdString(string);
  char *writeable = new char[stdstring.size() + 1];
  std::copy(stdstring.begin(), stdstring.end(), writeable);
  writeable[stdstring.size()] = '\0';
  return writeable;
}

// bind all of these functions into a compiler
void bindHiDefs(hobbes::cc &c) {
  using namespace hobbes;
  c.bind("enableColors", &enableConsoleCmds);
  c.bind("colors", &colors);

  c.bind("writefile", &writefile);
  c.bind("removefile", &removefile);
  c.bind("pexec", &pexec);
  c.bind("openfd", &openfd);
  c.bind("closefd", &closefd);
  c.bind("fdReadLine", &fdReadLine);
  c.bind("runPath", &runPath);

  c.bind("tick", &hobbes::tick);
  c.bind("showTick", &showTick);
}

void bindGLFWDefs(hobbes::cc &ctx) {
  ctx.bind("glfwInit", &glfwInit);
  ctx.bind("createWindow", &createWindow);
  ctx.bind("makeContextCurrent", &makeContextCurrent);
}

void bindTensorDefs(hobbes::cc &ctx) {
  ctx.bind("foreignString", &foreignString);
  // Memory File I/O
  ctx.bind("THMemoryFile_newWithStorage", &THMemoryFile_newWithStorage);
  ctx.bind("THMemoryFile_new", &THMemoryFile_new);
  ctx.bind("THMemoryFile_storage", &THMemoryFile_storage);
  ctx.bind("THMemoryFile_longSize", &THMemoryFile_longSize);
  // Disk File I/O
  ctx.bind("THDiskFile_new", &*THDiskFile_new);
  ctx.bind("THPipeFile_new", &*THPipeFile_new);
  ctx.bind("THDiskFile_name", &*THDiskFile_name);
  ctx.bind("THDiskFile_isLittleEndianCPU", &THDiskFile_isLittleEndianCPU);
  ctx.bind("THDiskFile_isBigEndianCPU", &THDiskFile_isBigEndianCPU);
  ctx.bind("THDiskFile_nativeEndianEncoding", &THDiskFile_nativeEndianEncoding);
  ctx.bind("THDiskFile_littleEndianEncoding", &THDiskFile_littleEndianEncoding);
  ctx.bind("THDiskFile_bigEndianEncoding", &THDiskFile_bigEndianEncoding);
  // generic file operations
  ctx.bind("THFile_isOpened", &THFile_isOpened);
  ctx.bind("THFile_isQuiet", &THFile_isQuiet);
  ctx.bind("THFile_isReadable", &THFile_isReadable);
  ctx.bind("THFile_isWritable", &THFile_isWritable);
  ctx.bind("THFile_isBinary", &THFile_isBinary);
  ctx.bind("THFile_isAutoSpacing", &THFile_isAutoSpacing);
  ctx.bind("THFile_hasError", &THFile_hasError);
  ctx.bind("THFile_binary", &THFile_binary);
  ctx.bind("THFile_ascii", &THFile_ascii);
  ctx.bind("THFile_autoSpacing", &THFile_autoSpacing);
  ctx.bind("THFile_noAutoSpacing", &THFile_noAutoSpacing);
  ctx.bind("THFile_quiet", &THFile_quiet);
  ctx.bind("THFile_pedantic", &THFile_pedantic);
  ctx.bind("THFile_clearError", &THFile_clearError);
  ctx.bind("THFile_readByteScalar", &THFile_readByteScalar);
  ctx.bind("THFile_readCharScalar", &THFile_readCharScalar);
  ctx.bind("THFile_readShortScalar", &THFile_readShortScalar);
  ctx.bind("THFile_readIntScalar", &THFile_readIntScalar);
  ctx.bind("THFile_readLongScalar", &THFile_readLongScalar);
  ctx.bind("THFile_readFloatScalar", &THFile_readFloatScalar);
  ctx.bind("THFile_readDoubleScalar", &THFile_readDoubleScalar);
  ctx.bind("THFile_writeByteScalar", &THFile_writeByteScalar);
  ctx.bind("THFile_writeCharScalar", &THFile_writeCharScalar);
  ctx.bind("THFile_writeShortScalar", &THFile_writeShortScalar);
  ctx.bind("THFile_writeIntScalar", &THFile_writeIntScalar);
  ctx.bind("THFile_writeLongScalar", &THFile_writeLongScalar);
  ctx.bind("THFile_writeFloatScalar", &THFile_writeFloatScalar);
  ctx.bind("THFile_writeDoubleScalar", &THFile_writeDoubleScalar);
  ctx.bind("THFile_readByte", &THFile_readByte);
  ctx.bind("THFile_readChar", &THFile_readChar);
  ctx.bind("THFile_readShort", &THFile_readShort);
  ctx.bind("THFile_readInt", &THFile_readInt);
  ctx.bind("THFile_readLong", &THFile_readLong);
  ctx.bind("THFile_readFloat", &THFile_readFloat);
  ctx.bind("THFile_readDouble", &THFile_readDouble);
  ctx.bind("THFile_writeByte", &THFile_writeByte);
  ctx.bind("THFile_writeChar", &THFile_writeChar);
  ctx.bind("THFile_writeShort", &THFile_writeShort);
  ctx.bind("THFile_writeInt", &THFile_writeInt);
  ctx.bind("THFile_writeLong", &THFile_writeLong);
  ctx.bind("THFile_writeFloat", &THFile_writeFloat);
  ctx.bind("THFile_writeDouble", &THFile_writeDouble);
  ctx.bind("THFile_readByteRaw", &THFile_readByteRaw);
  ctx.bind("THFile_readCharRaw", &THFile_readCharRaw);
  ctx.bind("THFile_readShortRaw", &THFile_readShortRaw);
  ctx.bind("THFile_readIntRaw", &THFile_readIntRaw);
  ctx.bind("THFile_readLongRaw", &THFile_readLongRaw);
  ctx.bind("THFile_readFloatRaw", &THFile_readFloatRaw);
  ctx.bind("THFile_readDoubleRaw", &THFile_readDoubleRaw);
  ctx.bind("THFile_readStringRaw", &THFile_readStringRaw);
  ctx.bind("THFile_writeByteRaw", &THFile_writeByteRaw);
  ctx.bind("THFile_writeCharRaw", &THFile_writeCharRaw);
  ctx.bind("THFile_writeShortRaw", &THFile_writeShortRaw);
  ctx.bind("THFile_writeIntRaw", &THFile_writeIntRaw);
  ctx.bind("THFile_writeLongRaw", &THFile_writeLongRaw);
  ctx.bind("THFile_writeFloatRaw", &THFile_writeFloatRaw);
  ctx.bind("THFile_writeDoubleRaw", &THFile_writeDoubleRaw);
  ctx.bind("THFile_writeStringRaw", &THFile_writeStringRaw);
  ctx.bind("THFile_readHalfScalar", &THFile_readHalfScalar);
  ctx.bind("THFile_writeHalfScalar", &THFile_writeHalfScalar);
  ctx.bind("THFile_readHalf", &THFile_readHalf);
  ctx.bind("THFile_writeHalf", &THFile_writeHalf);
  ctx.bind("THFile_readHalfRaw", &THFile_readHalfRaw);
  ctx.bind("THFile_writeHalfRaw", &THFile_writeHalfRaw);
  ctx.bind("THFile_synchronize", &THFile_synchronize);
  ctx.bind("THFile_seek", &THFile_seek);
  ctx.bind("THFile_seekEnd", &THFile_seekEnd);
  ctx.bind("THFile_position", &THFile_position);
  ctx.bind("THFile_close", &THFile_close);
  ctx.bind("THFile_free", &THFile_free);
  // Float
  // Float Tensor access
  ctx.bind("THFloatTensor_storage", &THFloatTensor_storage);
  ctx.bind("THFloatTensor_storageOffset", &THFloatTensor_storageOffset);
  ctx.bind("THFloatTensor_nDimension", &THFloatTensor_nDimension);
  ctx.bind("THFloatTensor_size", &THFloatTensor_size);
  ctx.bind("THFloatTensor_stride", &THFloatTensor_stride);
  ctx.bind("THFloatTensor_newSizeOf", &THFloatTensor_newSizeOf);
  ctx.bind("THFloatTensor_newStrideOf", &THFloatTensor_newStrideOf);
  ctx.bind("THFloatTensor_data", &THFloatTensor_data);
  ctx.bind("THFloatTensor_setFlag", &THFloatTensor_setFlag);
  ctx.bind("THFloatTensor_clearFlag", &THFloatTensor_clearFlag);
  // Float Tensor allocation
  ctx.bind("THFloatTensor_new", &THFloatTensor_new);
  ctx.bind("THFloatTensor_newWithTensor", &THFloatTensor_newWithTensor);
  ctx.bind("THFloatTensor_newWithStorage1d", &THFloatTensor_newWithStorage1d);
  ctx.bind("THFloatTensor_newWithStorage2d", &THFloatTensor_newWithStorage2d);
  ctx.bind("THFloatTensor_newWithStorage3d", &THFloatTensor_newWithStorage3d);
  ctx.bind("THFloatTensor_newWithStorage4d", &THFloatTensor_newWithStorage4d);
  ctx.bind("THFloatTensor_newWithSize1d", &THFloatTensor_newWithSize1d);
  ctx.bind("THFloatTensor_newWithSize2d", &THFloatTensor_newWithSize2d);
  ctx.bind("THFloatTensor_newWithSize3d", &THFloatTensor_newWithSize3d);
  ctx.bind("THFloatTensor_newWithSize4d", &THFloatTensor_newWithSize4d);
  ctx.bind("THFloatTensor_newClone", &THFloatTensor_newClone);
  ctx.bind("THFloatTensor_newContiguous", &THFloatTensor_newContiguous);
  ctx.bind("THFloatTensor_newSelect", &THFloatTensor_newSelect);
  ctx.bind("THFloatTensor_newNarrow", &THFloatTensor_newNarrow);
  ctx.bind("THFloatTensor_newTranspose", &THFloatTensor_newTranspose);
  ctx.bind("THFloatTensor_newUnfold", &THFloatTensor_newUnfold);
  ctx.bind("THFloatTensor_newView", &THFloatTensor_newView);
  ctx.bind("THFloatTensor_newExpand", &THFloatTensor_newExpand);
  ctx.bind("THFloatTensor_expand", &THFloatTensor_expand);
  ctx.bind("THFloatTensor_expandNd", &THFloatTensor_expandNd);
  ctx.bind("THFloatTensor_resize", &THFloatTensor_resize);
  ctx.bind("THFloatTensor_resizeAs", &THFloatTensor_resizeAs);
  ctx.bind("THFloatTensor_resize1d", &THFloatTensor_resize1d);
  ctx.bind("THFloatTensor_resize2d", &THFloatTensor_resize2d);
  ctx.bind("THFloatTensor_resize3d", &THFloatTensor_resize3d);
  ctx.bind("THFloatTensor_resize4d", &THFloatTensor_resize4d);
  ctx.bind("THFloatTensor_resize5d", &THFloatTensor_resize5d);
  ctx.bind("THFloatTensor_resizeNd", &THFloatTensor_resizeNd);
  ctx.bind("THFloatTensor_set", &THFloatTensor_set);
  ctx.bind("THFloatTensor_setStorage", &THFloatTensor_setStorage);
  ctx.bind("THFloatTensor_setStorageNd", &THFloatTensor_setStorageNd);
  ctx.bind("THFloatTensor_setStorage1d", &THFloatTensor_setStorage1d);
  ctx.bind("THFloatTensor_setStorage2d", &THFloatTensor_setStorage2d);
  ctx.bind("THFloatTensor_setStorage3d", &THFloatTensor_setStorage3d);
  ctx.bind("THFloatTensor_setStorage4d", &THFloatTensor_setStorage4d);
  ctx.bind("THFloatTensor_narrow", &THFloatTensor_narrow);
  ctx.bind("THFloatTensor_select", &THFloatTensor_select);
  ctx.bind("THFloatTensor_transpose", &THFloatTensor_transpose);
  ctx.bind("THFloatTensor_unfold", &THFloatTensor_unfold);
  ctx.bind("THFloatTensor_squeeze", &THFloatTensor_squeeze);
  ctx.bind("THFloatTensor_squeeze1d", &THFloatTensor_squeeze1d);
  ctx.bind("THFloatTensor_unsqueeze1d", &THFloatTensor_unsqueeze1d);
  ctx.bind("THFloatTensor_isContiguous", &THFloatTensor_isContiguous);
  ctx.bind("THFloatTensor_isSameSizeAs", &THFloatTensor_isSameSizeAs);
  ctx.bind("THFloatTensor_isSetTo", &THFloatTensor_isSetTo);
  ctx.bind("THFloatTensor_isSize", &THFloatTensor_isSize);
  ctx.bind("THFloatTensor_nElement", &THFloatTensor_nElement);
  ctx.bind("THFloatTensor_retain", &THFloatTensor_retain);
  ctx.bind("THFloatTensor_free", &THFloatTensor_free);
  ctx.bind("THFloatTensor_freeCopyTo", &THFloatTensor_freeCopyTo);
  // Float Tensor slow access
  ctx.bind("THFloatTensor_set1d", &THFloatTensor_set1d);
  ctx.bind("THFloatTensor_set2d", &THFloatTensor_set2d);
  ctx.bind("THFloatTensor_set3d", &THFloatTensor_set3d);
  ctx.bind("THFloatTensor_set4d", &THFloatTensor_set4d);
  ctx.bind("THFloatTensor_get1d", &THFloatTensor_get1d);
  ctx.bind("THFloatTensor_get2d", &THFloatTensor_get2d);
  ctx.bind("THFloatTensor_get3d", &THFloatTensor_get3d);
  ctx.bind("THFloatTensor_get4d", &THFloatTensor_get4d);
  // Float Tensor Math
  ctx.bind("THFloatTensor_fill", &THFloatTensor_fill);
  ctx.bind("THFloatTensor_zero", &THFloatTensor_zero);
  ctx.bind("THFloatTensor_maskedFill", &THFloatTensor_maskedFill);
  ctx.bind("THFloatTensor_maskedCopy", &THFloatTensor_maskedCopy);
  ctx.bind("THFloatTensor_maskedSelect", &THFloatTensor_maskedSelect);
  ctx.bind("THFloatTensor_nonzero", &THFloatTensor_nonzero);
  ctx.bind("THFloatTensor_indexSelect", &THFloatTensor_indexSelect);
  ctx.bind("THFloatTensor_indexCopy", &THFloatTensor_indexCopy);
  ctx.bind("THFloatTensor_indexAdd", &THFloatTensor_indexAdd);
  ctx.bind("THFloatTensor_indexFill", &THFloatTensor_indexFill);
  ctx.bind("THFloatTensor_gather", &THFloatTensor_gather);
  ctx.bind("THFloatTensor_scatter", &THFloatTensor_scatter);
  ctx.bind("THFloatTensor_scatterAdd", &THFloatTensor_scatterAdd);
  ctx.bind("THFloatTensor_scatterFill", &THFloatTensor_scatterFill);
  ctx.bind("THFloatTensor_dot", &THFloatTensor_dot);
  ctx.bind("THFloatTensor_minall", &THFloatTensor_minall);
  ctx.bind("THFloatTensor_maxall", &THFloatTensor_maxall);
  ctx.bind("THFloatTensor_medianall", &THFloatTensor_medianall);
  ctx.bind("THFloatTensor_sumall", &THFloatTensor_sumall);
  ctx.bind("THFloatTensor_prodall", &THFloatTensor_prodall);
  ctx.bind("THFloatTensor_neg", &THFloatTensor_neg);
  ctx.bind("THFloatTensor_cinv", &THFloatTensor_cinv);
  ctx.bind("THFloatTensor_add", &THFloatTensor_add);
  ctx.bind("THFloatTensor_sub", &THFloatTensor_sub);
  ctx.bind("THFloatTensor_mul", &THFloatTensor_mul);
  ctx.bind("THFloatTensor_div", &THFloatTensor_div);
  ctx.bind("THFloatTensor_lshift", &THFloatTensor_lshift);
  ctx.bind("THFloatTensor_rshift", &THFloatTensor_rshift);
  ctx.bind("THFloatTensor_fmod", &THFloatTensor_fmod);
  ctx.bind("THFloatTensor_remainder", &THFloatTensor_remainder);
  ctx.bind("THFloatTensor_clamp", &THFloatTensor_clamp);
  ctx.bind("THFloatTensor_bitand", &THFloatTensor_bitand);
  ctx.bind("THFloatTensor_bitor", &THFloatTensor_bitor);
  ctx.bind("THFloatTensor_bitxor", &THFloatTensor_bitxor);
  ctx.bind("THFloatTensor_cadd", &THFloatTensor_cadd);
  ctx.bind("THFloatTensor_csub", &THFloatTensor_csub);
  ctx.bind("THFloatTensor_cmul", &THFloatTensor_cmul);
  ctx.bind("THFloatTensor_cpow", &THFloatTensor_cpow);
  ctx.bind("THFloatTensor_cdiv", &THFloatTensor_cdiv);
  ctx.bind("THFloatTensor_clshift", &THFloatTensor_clshift);
  ctx.bind("THFloatTensor_crshift", &THFloatTensor_crshift);
  ctx.bind("THFloatTensor_cfmod", &THFloatTensor_cfmod);
  ctx.bind("THFloatTensor_cremainder", &THFloatTensor_cremainder);
  ctx.bind("THFloatTensor_cbitand", &THFloatTensor_cbitand);
  ctx.bind("THFloatTensor_cbitor", &THFloatTensor_cbitor);
  ctx.bind("THFloatTensor_cbitxor", &THFloatTensor_cbitxor);
  ctx.bind("THFloatTensor_addcmul", &THFloatTensor_addcmul);
  ctx.bind("THFloatTensor_addcdiv", &THFloatTensor_addcdiv);
  ctx.bind("THFloatTensor_addmv", &THFloatTensor_addmv);
  ctx.bind("THFloatTensor_addmm", &THFloatTensor_addmm);
  ctx.bind("THFloatTensor_addr", &THFloatTensor_addr);
  ctx.bind("THFloatTensor_addbmm", &THFloatTensor_addbmm);
  ctx.bind("THFloatTensor_baddbmm", &THFloatTensor_baddbmm);
  ctx.bind("THFloatTensor_match", &THFloatTensor_match);
  ctx.bind("THFloatTensor_numel", &THFloatTensor_numel);
  ctx.bind("THFloatTensor_max", &THFloatTensor_max);
  ctx.bind("THFloatTensor_min", &THFloatTensor_min);
  ctx.bind("THFloatTensor_kthvalue", &THFloatTensor_kthvalue);
  ctx.bind("THFloatTensor_mode", &THFloatTensor_mode);
  ctx.bind("THFloatTensor_median", &THFloatTensor_median);
  ctx.bind("THFloatTensor_sum", &THFloatTensor_sum);
  ctx.bind("THFloatTensor_prod", &THFloatTensor_prod);
  ctx.bind("THFloatTensor_cumsum", &THFloatTensor_cumsum);
  ctx.bind("THFloatTensor_cumprod", &THFloatTensor_cumprod);
  ctx.bind("THFloatTensor_sign", &THFloatTensor_sign);
  ctx.bind("THFloatTensor_trace", &THFloatTensor_trace);
  ctx.bind("THFloatTensor_cross", &THFloatTensor_cross);
  ctx.bind("THFloatTensor_cmax", &THFloatTensor_cmax);
  ctx.bind("THFloatTensor_cmin", &THFloatTensor_cmin);
  ctx.bind("THFloatTensor_cmaxValue", &THFloatTensor_cmaxValue);
  ctx.bind("THFloatTensor_cminValue", &THFloatTensor_cminValue);
  ctx.bind("THFloatTensor_zeros", &THFloatTensor_zeros);
  ctx.bind("THFloatTensor_zerosLike", &THFloatTensor_zerosLike);
  ctx.bind("THFloatTensor_ones", &THFloatTensor_ones);
  ctx.bind("THFloatTensor_onesLike", &THFloatTensor_onesLike);
  ctx.bind("THFloatTensor_diag", &THFloatTensor_diag);
  ctx.bind("THFloatTensor_eye", &THFloatTensor_eye);
  ctx.bind("THFloatTensor_arange", &THFloatTensor_arange);
  ctx.bind("THFloatTensor_range", &THFloatTensor_range);
  ctx.bind("THFloatTensor_randperm", &THFloatTensor_randperm);
  ctx.bind("THFloatTensor_reshape", &THFloatTensor_reshape);
  ctx.bind("THFloatTensor_sort", &THFloatTensor_sort);
  ctx.bind("THFloatTensor_topk", &THFloatTensor_topk);
  ctx.bind("THFloatTensor_tril", &THFloatTensor_tril);
  ctx.bind("THFloatTensor_triu", &THFloatTensor_triu);
  ctx.bind("THFloatTensor_cat", &THFloatTensor_cat);
  ctx.bind("THFloatTensor_catArray", &THFloatTensor_catArray);
  ctx.bind("THFloatTensor_equal", &THFloatTensor_equal);
  ctx.bind("THFloatTensor_ltValue", &THFloatTensor_ltValue);
  ctx.bind("THFloatTensor_leValue", &THFloatTensor_leValue);
  ctx.bind("THFloatTensor_gtValue", &THFloatTensor_gtValue);
  ctx.bind("THFloatTensor_geValue", &THFloatTensor_geValue);
  ctx.bind("THFloatTensor_neValue", &THFloatTensor_neValue);
  ctx.bind("THFloatTensor_eqValue", &THFloatTensor_eqValue);
  ctx.bind("THFloatTensor_ltValueT", &THFloatTensor_ltValueT);
  ctx.bind("THFloatTensor_leValueT", &THFloatTensor_leValueT);
  ctx.bind("THFloatTensor_gtValueT", &THFloatTensor_gtValueT);
  ctx.bind("THFloatTensor_geValueT", &THFloatTensor_geValueT);
  ctx.bind("THFloatTensor_neValueT", &THFloatTensor_neValueT);
  ctx.bind("THFloatTensor_eqValueT", &THFloatTensor_eqValueT);
  ctx.bind("THFloatTensor_ltTensor", &THFloatTensor_ltTensor);
  ctx.bind("THFloatTensor_leTensor", &THFloatTensor_leTensor);
  ctx.bind("THFloatTensor_gtTensor", &THFloatTensor_gtTensor);
  ctx.bind("THFloatTensor_geTensor", &THFloatTensor_geTensor);
  ctx.bind("THFloatTensor_neTensor", &THFloatTensor_neTensor);
  ctx.bind("THFloatTensor_eqTensor", &THFloatTensor_eqTensor);
  ctx.bind("THFloatTensor_ltTensorT", &THFloatTensor_ltTensorT);
  ctx.bind("THFloatTensor_leTensorT", &THFloatTensor_leTensorT);
  ctx.bind("THFloatTensor_gtTensorT", &THFloatTensor_gtTensorT);
  ctx.bind("THFloatTensor_geTensorT", &THFloatTensor_geTensorT);
  ctx.bind("THFloatTensor_neTensorT", &THFloatTensor_neTensorT);
  ctx.bind("THFloatTensor_eqTensorT", &THFloatTensor_eqTensorT);
  ctx.bind("THFloatTensor_sigmoid", &THFloatTensor_sigmoid);
  ctx.bind("THFloatTensor_log", &THFloatTensor_log);
  ctx.bind("THFloatTensor_lgamma", &THFloatTensor_lgamma);
  ctx.bind("THFloatTensor_log1p", &THFloatTensor_log1p);
  ctx.bind("THFloatTensor_exp", &THFloatTensor_exp);
  ctx.bind("THFloatTensor_cos", &THFloatTensor_cos);
  ctx.bind("THFloatTensor_acos", &THFloatTensor_acos);
  ctx.bind("THFloatTensor_cosh", &THFloatTensor_cosh);
  ctx.bind("THFloatTensor_sin", &THFloatTensor_sin);
  ctx.bind("THFloatTensor_asin", &THFloatTensor_asin);
  ctx.bind("THFloatTensor_sinh", &THFloatTensor_sinh);
  ctx.bind("THFloatTensor_tan", &THFloatTensor_tan);
  ctx.bind("THFloatTensor_atan", &THFloatTensor_atan);
  ctx.bind("THFloatTensor_atan2", &THFloatTensor_atan2);
  ctx.bind("THFloatTensor_tanh", &THFloatTensor_tanh);
  ctx.bind("THFloatTensor_pow", &THFloatTensor_pow);
  ctx.bind("THFloatTensor_tpow", &THFloatTensor_tpow);
  ctx.bind("THFloatTensor_sqrt", &THFloatTensor_sqrt);
  ctx.bind("THFloatTensor_rsqrt", &THFloatTensor_rsqrt);
  ctx.bind("THFloatTensor_ceil", &THFloatTensor_ceil);
  ctx.bind("THFloatTensor_floor", &THFloatTensor_floor);
  ctx.bind("THFloatTensor_round", &THFloatTensor_round);
  ctx.bind("THFloatTensor_abs", &THFloatTensor_abs);
  ctx.bind("THFloatTensor_trunc", &THFloatTensor_trunc);
  ctx.bind("THFloatTensor_frac", &THFloatTensor_frac);
  ctx.bind("THFloatTensor_lerp", &THFloatTensor_lerp);
  ctx.bind("THFloatTensor_mean", &THFloatTensor_mean);
  ctx.bind("THFloatTensor_std", &THFloatTensor_std);
  ctx.bind("THFloatTensor_var", &THFloatTensor_var);
  ctx.bind("THFloatTensor_norm", &THFloatTensor_norm);
  ctx.bind("THFloatTensor_renorm", &THFloatTensor_renorm);
  ctx.bind("THFloatTensor_dist", &THFloatTensor_dist);
  ctx.bind("THFloatTensor_histc", &THFloatTensor_histc);
  ctx.bind("THFloatTensor_bhistc", &THFloatTensor_bhistc);
  ctx.bind("THFloatTensor_meanall", &THFloatTensor_meanall);
  ctx.bind("THFloatTensor_varall", &THFloatTensor_varall);
  ctx.bind("THFloatTensor_stdall", &THFloatTensor_stdall);
  ctx.bind("THFloatTensor_normall", &THFloatTensor_normall);
  ctx.bind("THFloatTensor_linspace", &THFloatTensor_linspace);
  ctx.bind("THFloatTensor_logspace", &THFloatTensor_logspace);
  ctx.bind("THFloatTensor_rand", &THFloatTensor_rand);
  ctx.bind("THFloatTensor_randn", &THFloatTensor_randn);
  // Float Random
  ctx.bind("THFloatTensor_random", &THFloatTensor_random);
  ctx.bind("THFloatTensor_clampedRandom", &THFloatTensor_clampedRandom);
  ctx.bind("THFloatTensor_cappedRandom", &THFloatTensor_cappedRandom);
  ctx.bind("THFloatTensor_geometric", &THFloatTensor_geometric);
  ctx.bind("THFloatTensor_bernoulli", &THFloatTensor_bernoulli);
  ctx.bind("THFloatTensor_bernoulli_FloatTensor",
           &THFloatTensor_bernoulli_FloatTensor);
  ctx.bind("THFloatTensor_bernoulli_DoubleTensor",
           &THFloatTensor_bernoulli_DoubleTensor);
  ctx.bind("THFloatTensor_uniform", &THFloatTensor_uniform);
  ctx.bind("THFloatTensor_normal", &THFloatTensor_normal);
  ctx.bind("THFloatTensor_normal_means", &THFloatTensor_normal_means);
  ctx.bind("THFloatTensor_normal_stddevs", &THFloatTensor_normal_stddevs);
  ctx.bind("THFloatTensor_normal_means_stddevs",
           &THFloatTensor_normal_means_stddevs);
  ctx.bind("THFloatTensor_exponential", &THFloatTensor_exponential);
  ctx.bind("THFloatTensor_cauchy", &THFloatTensor_cauchy);
  ctx.bind("THFloatTensor_logNormal", &THFloatTensor_logNormal);
  ctx.bind("THFloatTensor_multinomial", &THFloatTensor_multinomial);
  ctx.bind("THFloatTensor_multinomialAliasSetup",
           &THFloatTensor_multinomialAliasSetup);
  ctx.bind("THFloatTensor_multinomialAliasDraw",
           &THFloatTensor_multinomialAliasDraw);
  // Float Storage
  ctx.bind("THFloatStorage_new", &THFloatStorage_new);
  ctx.bind("THFloatStorage_clearFlag", &THFloatStorage_clearFlag);
  ctx.bind("THFloatStorage_copy", &THFloatStorage_copy);
  ctx.bind("THFloatStorage_copyByte", &THFloatStorage_copyByte);
  ctx.bind("THFloatStorage_copyChar", &THFloatStorage_copyChar);
  ctx.bind("THFloatStorage_copyDouble", &THFloatStorage_copyDouble);
  ctx.bind("THFloatStorage_copyFloat", &THFloatStorage_copyFloat);
  ctx.bind("THFloatStorage_copyHalf", &THFloatStorage_copyHalf);
  ctx.bind("THFloatStorage_copyInt", &THFloatStorage_copyInt);
  ctx.bind("THFloatStorage_copyLong", &THFloatStorage_copyLong);
  ctx.bind("THFloatStorage_copyShort", &THFloatStorage_copyShort);
  ctx.bind("THFloatStorage_data", &THFloatStorage_data);
  ctx.bind("THFloatStorage_elementSize", &THFloatStorage_elementSize);
  ctx.bind("THFloatStorage_fill", &THFloatStorage_fill);
  ctx.bind("THFloatStorage_free", &THFloatStorage_free);
  ctx.bind("THFloatStorage_get", &THFloatStorage_get);
  ctx.bind("THFloatStorage_newWithAllocator", &THFloatStorage_newWithAllocator);
  ctx.bind("THFloatStorage_newWithData", &THFloatStorage_newWithData);
  ctx.bind("THFloatStorage_newWithDataAndAllocator",
           &THFloatStorage_newWithDataAndAllocator);
  ctx.bind("THFloatStorage_newWithMapping", &THFloatStorage_newWithMapping);
  ctx.bind("THFloatStorage_newWithSize", &THFloatStorage_newWithSize);
  ctx.bind("THFloatStorage_newWithSize1", &THFloatStorage_newWithSize1);
  ctx.bind("THFloatStorage_newWithSize2", &THFloatStorage_newWithSize2);
  ctx.bind("THFloatStorage_newWithSize3", &THFloatStorage_newWithSize3);
  ctx.bind("THFloatStorage_newWithSize4", &THFloatStorage_newWithSize4);
  ctx.bind("THFloatStorage_rawCopy", &THFloatStorage_rawCopy);
  ctx.bind("THFloatStorage_resize", &THFloatStorage_resize);
  ctx.bind("THFloatStorage_retain", &THFloatStorage_retain);
  ctx.bind("THFloatStorage_set", &THFloatStorage_set);
  ctx.bind("THFloatStorage_setFlag", &THFloatStorage_setFlag);
  ctx.bind("THFloatStorage_size", &THFloatStorage_size);
  ctx.bind("THFloatStorage_swap", &THFloatStorage_swap);
  // Convolution
  ctx.bind("THFloatTensor_validXCorr2Dptr", &THFloatTensor_validXCorr2Dptr);
  ctx.bind("THFloatTensor_validConv2Dptr", &THFloatTensor_validConv2Dptr);
  ctx.bind("THFloatTensor_fullXCorr2Dptr", &THFloatTensor_fullXCorr2Dptr);
  ctx.bind("THFloatTensor_fullConv2Dptr", &THFloatTensor_fullConv2Dptr);
  ctx.bind("THFloatTensor_validXCorr2DRevptr", &THFloatTensor_validXCorr2DRevptr);
  ctx.bind("THFloatTensor_conv2DRevger", &THFloatTensor_conv2DRevger);
  ctx.bind("THFloatTensor_conv2DRevgerm", &THFloatTensor_conv2DRevgerm);
  ctx.bind("THFloatTensor_conv2Dger", &THFloatTensor_conv2Dger);
  ctx.bind("THFloatTensor_conv2Dmv", &THFloatTensor_conv2Dmv);
  ctx.bind("THFloatTensor_conv2Dmm", &THFloatTensor_conv2Dmm);
  ctx.bind("THFloatTensor_conv2Dmul", &THFloatTensor_conv2Dmul);
  ctx.bind("THFloatTensor_conv2Dcmul", &THFloatTensor_conv2Dcmul);
  ctx.bind("THFloatTensor_validXCorr3Dptr", &THFloatTensor_validXCorr3Dptr);
  ctx.bind("THFloatTensor_validConv3Dptr", &THFloatTensor_validConv3Dptr);
  ctx.bind("THFloatTensor_fullXCorr3Dptr", &THFloatTensor_fullXCorr3Dptr);
  ctx.bind("THFloatTensor_fullConv3Dptr", &THFloatTensor_fullConv3Dptr);
  ctx.bind("THFloatTensor_validXCorr3DRevptr", &THFloatTensor_validXCorr3DRevptr);
  ctx.bind("THFloatTensor_conv3DRevger", &THFloatTensor_conv3DRevger);
  ctx.bind("THFloatTensor_conv3Dger", &THFloatTensor_conv3Dger);
  ctx.bind("THFloatTensor_conv3Dmv", &THFloatTensor_conv3Dmv);
  ctx.bind("THFloatTensor_conv3Dmul", &THFloatTensor_conv3Dmul);
  ctx.bind("THFloatTensor_conv3Dcmul", &THFloatTensor_conv3Dcmul);
    
  // Double
  // Double Tensor access
  ctx.bind("THDoubleTensor_storage", &THDoubleTensor_storage);
  ctx.bind("THDoubleTensor_storageOffset", &THDoubleTensor_storageOffset);
  ctx.bind("THDoubleTensor_nDimension", &THDoubleTensor_nDimension);
  ctx.bind("THDoubleTensor_size", &THDoubleTensor_size);
  ctx.bind("THDoubleTensor_stride", &THDoubleTensor_stride);
  ctx.bind("THDoubleTensor_newSizeOf", &THDoubleTensor_newSizeOf);
  ctx.bind("THDoubleTensor_newStrideOf", &THDoubleTensor_newStrideOf);
  ctx.bind("THDoubleTensor_data", &THDoubleTensor_data);
  ctx.bind("THDoubleTensor_setFlag", &THDoubleTensor_setFlag);
  ctx.bind("THDoubleTensor_clearFlag", &THDoubleTensor_clearFlag);
  // Double Tensor allocation
  ctx.bind("THDoubleTensor_new", &THDoubleTensor_new);
  ctx.bind("THDoubleTensor_newWithTensor", &THDoubleTensor_newWithTensor);
  ctx.bind("THDoubleTensor_newWithStorage1d", &THDoubleTensor_newWithStorage1d);
  ctx.bind("THDoubleTensor_newWithStorage2d", &THDoubleTensor_newWithStorage2d);
  ctx.bind("THDoubleTensor_newWithStorage3d", &THDoubleTensor_newWithStorage3d);
  ctx.bind("THDoubleTensor_newWithStorage4d", &THDoubleTensor_newWithStorage4d);
  ctx.bind("THDoubleTensor_newWithSize1d", &THDoubleTensor_newWithSize1d);
  ctx.bind("THDoubleTensor_newWithSize2d", &THDoubleTensor_newWithSize2d);
  ctx.bind("THDoubleTensor_newWithSize3d", &THDoubleTensor_newWithSize3d);
  ctx.bind("THDoubleTensor_newWithSize4d", &THDoubleTensor_newWithSize4d);
  ctx.bind("THDoubleTensor_newClone", &THDoubleTensor_newClone);
  ctx.bind("THDoubleTensor_newContiguous", &THDoubleTensor_newContiguous);
  ctx.bind("THDoubleTensor_newSelect", &THDoubleTensor_newSelect);
  ctx.bind("THDoubleTensor_newNarrow", &THDoubleTensor_newNarrow);
  ctx.bind("THDoubleTensor_newTranspose", &THDoubleTensor_newTranspose);
  ctx.bind("THDoubleTensor_newUnfold", &THDoubleTensor_newUnfold);
  ctx.bind("THDoubleTensor_newView", &THDoubleTensor_newView);
  ctx.bind("THDoubleTensor_newExpand", &THDoubleTensor_newExpand);
  ctx.bind("THDoubleTensor_expand", &THDoubleTensor_expand);
  ctx.bind("THDoubleTensor_expandNd", &THDoubleTensor_expandNd);
  ctx.bind("THDoubleTensor_resize", &THDoubleTensor_resize);
  ctx.bind("THDoubleTensor_resizeAs", &THDoubleTensor_resizeAs);
  ctx.bind("THDoubleTensor_resize1d", &THDoubleTensor_resize1d);
  ctx.bind("THDoubleTensor_resize2d", &THDoubleTensor_resize2d);
  ctx.bind("THDoubleTensor_resize3d", &THDoubleTensor_resize3d);
  ctx.bind("THDoubleTensor_resize4d", &THDoubleTensor_resize4d);
  ctx.bind("THDoubleTensor_resize5d", &THDoubleTensor_resize5d);
  ctx.bind("THDoubleTensor_resizeNd", &THDoubleTensor_resizeNd);
  ctx.bind("THDoubleTensor_set", &THDoubleTensor_set);
  ctx.bind("THDoubleTensor_setStorage", &THDoubleTensor_setStorage);
  ctx.bind("THDoubleTensor_setStorageNd", &THDoubleTensor_setStorageNd);
  ctx.bind("THDoubleTensor_setStorage1d", &THDoubleTensor_setStorage1d);
  ctx.bind("THDoubleTensor_setStorage2d", &THDoubleTensor_setStorage2d);
  ctx.bind("THDoubleTensor_setStorage3d", &THDoubleTensor_setStorage3d);
  ctx.bind("THDoubleTensor_setStorage4d", &THDoubleTensor_setStorage4d);
  ctx.bind("THDoubleTensor_narrow", &THDoubleTensor_narrow);
  ctx.bind("THDoubleTensor_select", &THDoubleTensor_select);
  ctx.bind("THDoubleTensor_transpose", &THDoubleTensor_transpose);
  ctx.bind("THDoubleTensor_unfold", &THDoubleTensor_unfold);
  ctx.bind("THDoubleTensor_squeeze", &THDoubleTensor_squeeze);
  ctx.bind("THDoubleTensor_squeeze1d", &THDoubleTensor_squeeze1d);
  ctx.bind("THDoubleTensor_unsqueeze1d", &THDoubleTensor_unsqueeze1d);
  ctx.bind("THDoubleTensor_isContiguous", &THDoubleTensor_isContiguous);
  ctx.bind("THDoubleTensor_isSameSizeAs", &THDoubleTensor_isSameSizeAs);
  ctx.bind("THDoubleTensor_isSetTo", &THDoubleTensor_isSetTo);
  ctx.bind("THDoubleTensor_isSize", &THDoubleTensor_isSize);
  ctx.bind("THDoubleTensor_nElement", &THDoubleTensor_nElement);
  ctx.bind("THDoubleTensor_retain", &THDoubleTensor_retain);
  ctx.bind("THDoubleTensor_free", &THDoubleTensor_free);
  ctx.bind("THDoubleTensor_freeCopyTo", &THDoubleTensor_freeCopyTo);
  // Double Tensor slow access
  ctx.bind("THDoubleTensor_set1d", &THDoubleTensor_set1d);
  ctx.bind("THDoubleTensor_set2d", &THDoubleTensor_set2d);
  ctx.bind("THDoubleTensor_set3d", &THDoubleTensor_set3d);
  ctx.bind("THDoubleTensor_set4d", &THDoubleTensor_set4d);
  ctx.bind("THDoubleTensor_get1d", &THDoubleTensor_get1d);
  ctx.bind("THDoubleTensor_get2d", &THDoubleTensor_get2d);
  ctx.bind("THDoubleTensor_get3d", &THDoubleTensor_get3d);
  ctx.bind("THDoubleTensor_get4d", &THDoubleTensor_get4d);
  // Double Tensor Math
  ctx.bind("THDoubleTensor_fill", &THDoubleTensor_fill);
  ctx.bind("THDoubleTensor_zero", &THDoubleTensor_zero);
  ctx.bind("THDoubleTensor_maskedFill", &THDoubleTensor_maskedFill);
  ctx.bind("THDoubleTensor_maskedCopy", &THDoubleTensor_maskedCopy);
  ctx.bind("THDoubleTensor_maskedSelect", &THDoubleTensor_maskedSelect);
  ctx.bind("THDoubleTensor_nonzero", &THDoubleTensor_nonzero);
  ctx.bind("THDoubleTensor_indexSelect", &THDoubleTensor_indexSelect);
  ctx.bind("THDoubleTensor_indexCopy", &THDoubleTensor_indexCopy);
  ctx.bind("THDoubleTensor_indexAdd", &THDoubleTensor_indexAdd);
  ctx.bind("THDoubleTensor_indexFill", &THDoubleTensor_indexFill);
  ctx.bind("THDoubleTensor_gather", &THDoubleTensor_gather);
  ctx.bind("THDoubleTensor_scatter", &THDoubleTensor_scatter);
  ctx.bind("THDoubleTensor_scatterAdd", &THDoubleTensor_scatterAdd);
  ctx.bind("THDoubleTensor_scatterFill", &THDoubleTensor_scatterFill);
  ctx.bind("THDoubleTensor_dot", &THDoubleTensor_dot);
  ctx.bind("THDoubleTensor_minall", &THDoubleTensor_minall);
  ctx.bind("THDoubleTensor_maxall", &THDoubleTensor_maxall);
  ctx.bind("THDoubleTensor_medianall", &THDoubleTensor_medianall);
  ctx.bind("THDoubleTensor_sumall", &THDoubleTensor_sumall);
  ctx.bind("THDoubleTensor_prodall", &THDoubleTensor_prodall);
  ctx.bind("THDoubleTensor_neg", &THDoubleTensor_neg);
  ctx.bind("THDoubleTensor_cinv", &THDoubleTensor_cinv);
  ctx.bind("THDoubleTensor_add", &THDoubleTensor_add);
  ctx.bind("THDoubleTensor_sub", &THDoubleTensor_sub);
  ctx.bind("THDoubleTensor_mul", &THDoubleTensor_mul);
  ctx.bind("THDoubleTensor_div", &THDoubleTensor_div);
  ctx.bind("THDoubleTensor_lshift", &THDoubleTensor_lshift);
  ctx.bind("THDoubleTensor_rshift", &THDoubleTensor_rshift);
  ctx.bind("THDoubleTensor_fmod", &THDoubleTensor_fmod);
  ctx.bind("THDoubleTensor_remainder", &THDoubleTensor_remainder);
  ctx.bind("THDoubleTensor_clamp", &THDoubleTensor_clamp);
  ctx.bind("THDoubleTensor_bitand", &THDoubleTensor_bitand);
  ctx.bind("THDoubleTensor_bitor", &THDoubleTensor_bitor);
  ctx.bind("THDoubleTensor_bitxor", &THDoubleTensor_bitxor);
  ctx.bind("THDoubleTensor_cadd", &THDoubleTensor_cadd);
  ctx.bind("THDoubleTensor_csub", &THDoubleTensor_csub);
  ctx.bind("THDoubleTensor_cmul", &THDoubleTensor_cmul);
  ctx.bind("THDoubleTensor_cpow", &THDoubleTensor_cpow);
  ctx.bind("THDoubleTensor_cdiv", &THDoubleTensor_cdiv);
  ctx.bind("THDoubleTensor_clshift", &THDoubleTensor_clshift);
  ctx.bind("THDoubleTensor_crshift", &THDoubleTensor_crshift);
  ctx.bind("THDoubleTensor_cfmod", &THDoubleTensor_cfmod);
  ctx.bind("THDoubleTensor_cremainder", &THDoubleTensor_cremainder);
  ctx.bind("THDoubleTensor_cbitand", &THDoubleTensor_cbitand);
  ctx.bind("THDoubleTensor_cbitor", &THDoubleTensor_cbitor);
  ctx.bind("THDoubleTensor_cbitxor", &THDoubleTensor_cbitxor);
  ctx.bind("THDoubleTensor_addcmul", &THDoubleTensor_addcmul);
  ctx.bind("THDoubleTensor_addcdiv", &THDoubleTensor_addcdiv);
  ctx.bind("THDoubleTensor_addmv", &THDoubleTensor_addmv);
  ctx.bind("THDoubleTensor_addmm", &THDoubleTensor_addmm);
  ctx.bind("THDoubleTensor_addr", &THDoubleTensor_addr);
  ctx.bind("THDoubleTensor_addbmm", &THDoubleTensor_addbmm);
  ctx.bind("THDoubleTensor_baddbmm", &THDoubleTensor_baddbmm);
  ctx.bind("THDoubleTensor_match", &THDoubleTensor_match);
  ctx.bind("THDoubleTensor_numel", &THDoubleTensor_numel);
  ctx.bind("THDoubleTensor_max", &THDoubleTensor_max);
  ctx.bind("THDoubleTensor_min", &THDoubleTensor_min);
  ctx.bind("THDoubleTensor_kthvalue", &THDoubleTensor_kthvalue);
  ctx.bind("THDoubleTensor_mode", &THDoubleTensor_mode);
  ctx.bind("THDoubleTensor_median", &THDoubleTensor_median);
  ctx.bind("THDoubleTensor_sum", &THDoubleTensor_sum);
  ctx.bind("THDoubleTensor_prod", &THDoubleTensor_prod);
  ctx.bind("THDoubleTensor_cumsum", &THDoubleTensor_cumsum);
  ctx.bind("THDoubleTensor_cumprod", &THDoubleTensor_cumprod);
  ctx.bind("THDoubleTensor_sign", &THDoubleTensor_sign);
  ctx.bind("THDoubleTensor_trace", &THDoubleTensor_trace);
  ctx.bind("THDoubleTensor_cross", &THDoubleTensor_cross);
  ctx.bind("THDoubleTensor_cmax", &THDoubleTensor_cmax);
  ctx.bind("THDoubleTensor_cmin", &THDoubleTensor_cmin);
  ctx.bind("THDoubleTensor_cmaxValue", &THDoubleTensor_cmaxValue);
  ctx.bind("THDoubleTensor_cminValue", &THDoubleTensor_cminValue);
  ctx.bind("THDoubleTensor_zeros", &THDoubleTensor_zeros);
  ctx.bind("THDoubleTensor_zerosLike", &THDoubleTensor_zerosLike);
  ctx.bind("THDoubleTensor_ones", &THDoubleTensor_ones);
  ctx.bind("THDoubleTensor_onesLike", &THDoubleTensor_onesLike);
  ctx.bind("THDoubleTensor_diag", &THDoubleTensor_diag);
  ctx.bind("THDoubleTensor_eye", &THDoubleTensor_eye);
  ctx.bind("THDoubleTensor_arange", &THDoubleTensor_arange);
  ctx.bind("THDoubleTensor_range", &THDoubleTensor_range);
  ctx.bind("THDoubleTensor_randperm", &THDoubleTensor_randperm);
  ctx.bind("THDoubleTensor_reshape", &THDoubleTensor_reshape);
  ctx.bind("THDoubleTensor_sort", &THDoubleTensor_sort);
  ctx.bind("THDoubleTensor_topk", &THDoubleTensor_topk);
  ctx.bind("THDoubleTensor_tril", &THDoubleTensor_tril);
  ctx.bind("THDoubleTensor_triu", &THDoubleTensor_triu);
  ctx.bind("THDoubleTensor_cat", &THDoubleTensor_cat);
  ctx.bind("THDoubleTensor_catArray", &THDoubleTensor_catArray);
  ctx.bind("THDoubleTensor_equal", &THDoubleTensor_equal);
  ctx.bind("THDoubleTensor_ltValue", &THDoubleTensor_ltValue);
  ctx.bind("THDoubleTensor_leValue", &THDoubleTensor_leValue);
  ctx.bind("THDoubleTensor_gtValue", &THDoubleTensor_gtValue);
  ctx.bind("THDoubleTensor_geValue", &THDoubleTensor_geValue);
  ctx.bind("THDoubleTensor_neValue", &THDoubleTensor_neValue);
  ctx.bind("THDoubleTensor_eqValue", &THDoubleTensor_eqValue);
  ctx.bind("THDoubleTensor_ltValueT", &THDoubleTensor_ltValueT);
  ctx.bind("THDoubleTensor_leValueT", &THDoubleTensor_leValueT);
  ctx.bind("THDoubleTensor_gtValueT", &THDoubleTensor_gtValueT);
  ctx.bind("THDoubleTensor_geValueT", &THDoubleTensor_geValueT);
  ctx.bind("THDoubleTensor_neValueT", &THDoubleTensor_neValueT);
  ctx.bind("THDoubleTensor_eqValueT", &THDoubleTensor_eqValueT);
  ctx.bind("THDoubleTensor_ltTensor", &THDoubleTensor_ltTensor);
  ctx.bind("THDoubleTensor_leTensor", &THDoubleTensor_leTensor);
  ctx.bind("THDoubleTensor_gtTensor", &THDoubleTensor_gtTensor);
  ctx.bind("THDoubleTensor_geTensor", &THDoubleTensor_geTensor);
  ctx.bind("THDoubleTensor_neTensor", &THDoubleTensor_neTensor);
  ctx.bind("THDoubleTensor_eqTensor", &THDoubleTensor_eqTensor);
  ctx.bind("THDoubleTensor_ltTensorT", &THDoubleTensor_ltTensorT);
  ctx.bind("THDoubleTensor_leTensorT", &THDoubleTensor_leTensorT);
  ctx.bind("THDoubleTensor_gtTensorT", &THDoubleTensor_gtTensorT);
  ctx.bind("THDoubleTensor_geTensorT", &THDoubleTensor_geTensorT);
  ctx.bind("THDoubleTensor_neTensorT", &THDoubleTensor_neTensorT);
  ctx.bind("THDoubleTensor_eqTensorT", &THDoubleTensor_eqTensorT);
  ctx.bind("THDoubleTensor_sigmoid", &THDoubleTensor_sigmoid);
  ctx.bind("THDoubleTensor_log", &THDoubleTensor_log);
  ctx.bind("THDoubleTensor_lgamma", &THDoubleTensor_lgamma);
  ctx.bind("THDoubleTensor_log1p", &THDoubleTensor_log1p);
  ctx.bind("THDoubleTensor_exp", &THDoubleTensor_exp);
  ctx.bind("THDoubleTensor_cos", &THDoubleTensor_cos);
  ctx.bind("THDoubleTensor_acos", &THDoubleTensor_acos);
  ctx.bind("THDoubleTensor_cosh", &THDoubleTensor_cosh);
  ctx.bind("THDoubleTensor_sin", &THDoubleTensor_sin);
  ctx.bind("THDoubleTensor_asin", &THDoubleTensor_asin);
  ctx.bind("THDoubleTensor_sinh", &THDoubleTensor_sinh);
  ctx.bind("THDoubleTensor_tan", &THDoubleTensor_tan);
  ctx.bind("THDoubleTensor_atan", &THDoubleTensor_atan);
  ctx.bind("THDoubleTensor_atan2", &THDoubleTensor_atan2);
  ctx.bind("THDoubleTensor_tanh", &THDoubleTensor_tanh);
  ctx.bind("THDoubleTensor_pow", &THDoubleTensor_pow);
  ctx.bind("THDoubleTensor_tpow", &THDoubleTensor_tpow);
  ctx.bind("THDoubleTensor_sqrt", &THDoubleTensor_sqrt);
  ctx.bind("THDoubleTensor_rsqrt", &THDoubleTensor_rsqrt);
  ctx.bind("THDoubleTensor_ceil", &THDoubleTensor_ceil);
  ctx.bind("THDoubleTensor_floor", &THDoubleTensor_floor);
  ctx.bind("THDoubleTensor_round", &THDoubleTensor_round);
  ctx.bind("THDoubleTensor_abs", &THDoubleTensor_abs);
  ctx.bind("THDoubleTensor_trunc", &THDoubleTensor_trunc);
  ctx.bind("THDoubleTensor_frac", &THDoubleTensor_frac);
  ctx.bind("THDoubleTensor_lerp", &THDoubleTensor_lerp);
  ctx.bind("THDoubleTensor_mean", &THDoubleTensor_mean);
  ctx.bind("THDoubleTensor_std", &THDoubleTensor_std);
  ctx.bind("THDoubleTensor_var", &THDoubleTensor_var);
  ctx.bind("THDoubleTensor_norm", &THDoubleTensor_norm);
  ctx.bind("THDoubleTensor_renorm", &THDoubleTensor_renorm);
  ctx.bind("THDoubleTensor_dist", &THDoubleTensor_dist);
  ctx.bind("THDoubleTensor_histc", &THDoubleTensor_histc);
  ctx.bind("THDoubleTensor_bhistc", &THDoubleTensor_bhistc);
  ctx.bind("THDoubleTensor_meanall", &THDoubleTensor_meanall);
  ctx.bind("THDoubleTensor_varall", &THDoubleTensor_varall);
  ctx.bind("THDoubleTensor_stdall", &THDoubleTensor_stdall);
  ctx.bind("THDoubleTensor_normall", &THDoubleTensor_normall);
  ctx.bind("THDoubleTensor_linspace", &THDoubleTensor_linspace);
  ctx.bind("THDoubleTensor_logspace", &THDoubleTensor_logspace);
  ctx.bind("THDoubleTensor_rand", &THDoubleTensor_rand);
  ctx.bind("THDoubleTensor_randn", &THDoubleTensor_randn);
  // Double Random
  ctx.bind("THDoubleTensor_random", &THDoubleTensor_random);
  ctx.bind("THDoubleTensor_clampedRandom", &THDoubleTensor_clampedRandom);
  ctx.bind("THDoubleTensor_cappedRandom", &THDoubleTensor_cappedRandom);
  ctx.bind("THDoubleTensor_geometric", &THDoubleTensor_geometric);
  ctx.bind("THDoubleTensor_bernoulli", &THDoubleTensor_bernoulli);
  ctx.bind("THDoubleTensor_bernoulli_DoubleTensor",
           &THDoubleTensor_bernoulli_DoubleTensor);
  ctx.bind("THDoubleTensor_uniform", &THDoubleTensor_uniform);
  ctx.bind("THDoubleTensor_normal", &THDoubleTensor_normal);
  ctx.bind("THDoubleTensor_normal_means", &THDoubleTensor_normal_means);
  ctx.bind("THDoubleTensor_normal_stddevs", &THDoubleTensor_normal_stddevs);
  ctx.bind("THDoubleTensor_normal_means_stddevs",
           &THDoubleTensor_normal_means_stddevs);
  ctx.bind("THDoubleTensor_exponential", &THDoubleTensor_exponential);
  ctx.bind("THDoubleTensor_cauchy", &THDoubleTensor_cauchy);
  ctx.bind("THDoubleTensor_logNormal", &THDoubleTensor_logNormal);
  ctx.bind("THDoubleTensor_multinomial", &THDoubleTensor_multinomial);
  ctx.bind("THDoubleTensor_multinomialAliasSetup",
           &THDoubleTensor_multinomialAliasSetup);
  ctx.bind("THDoubleTensor_multinomialAliasDraw",
           &THDoubleTensor_multinomialAliasDraw);
  // Convolution
  ctx.bind("THDoubleTensor_validXCorr2Dptr", &THDoubleTensor_validXCorr2Dptr);
  ctx.bind("THDoubleTensor_validConv2Dptr", &THDoubleTensor_validConv2Dptr);
  ctx.bind("THDoubleTensor_fullXCorr2Dptr", &THDoubleTensor_fullXCorr2Dptr);
  ctx.bind("THDoubleTensor_fullConv2Dptr", &THDoubleTensor_fullConv2Dptr);
  ctx.bind("THDoubleTensor_validXCorr2DRevptr", &THDoubleTensor_validXCorr2DRevptr);
  ctx.bind("THDoubleTensor_conv2DRevger", &THDoubleTensor_conv2DRevger);
  ctx.bind("THDoubleTensor_conv2DRevgerm", &THDoubleTensor_conv2DRevgerm);
  ctx.bind("THDoubleTensor_conv2Dger", &THDoubleTensor_conv2Dger);
  ctx.bind("THDoubleTensor_conv2Dmv", &THDoubleTensor_conv2Dmv);
  ctx.bind("THDoubleTensor_conv2Dmm", &THDoubleTensor_conv2Dmm);
  ctx.bind("THDoubleTensor_conv2Dmul", &THDoubleTensor_conv2Dmul);
  ctx.bind("THDoubleTensor_conv2Dcmul", &THDoubleTensor_conv2Dcmul);
  ctx.bind("THDoubleTensor_validXCorr3Dptr", &THDoubleTensor_validXCorr3Dptr);
  ctx.bind("THDoubleTensor_validConv3Dptr", &THDoubleTensor_validConv3Dptr);
  ctx.bind("THDoubleTensor_fullXCorr3Dptr", &THDoubleTensor_fullXCorr3Dptr);
  ctx.bind("THDoubleTensor_fullConv3Dptr", &THDoubleTensor_fullConv3Dptr);
  ctx.bind("THDoubleTensor_validXCorr3DRevptr", &THDoubleTensor_validXCorr3DRevptr);
  ctx.bind("THDoubleTensor_conv3DRevger", &THDoubleTensor_conv3DRevger);
  ctx.bind("THDoubleTensor_conv3Dger", &THDoubleTensor_conv3Dger);
  ctx.bind("THDoubleTensor_conv3Dmv", &THDoubleTensor_conv3Dmv);
  ctx.bind("THDoubleTensor_conv3Dmul", &THDoubleTensor_conv3Dmul);
  ctx.bind("THDoubleTensor_conv3Dcmul", &THDoubleTensor_conv3Dcmul);
  
  // Long
  // Long Tensor access
  ctx.bind("THLongTensor_storage", &THLongTensor_storage);
  ctx.bind("THLongTensor_storageOffset", &THLongTensor_storageOffset);
  ctx.bind("THLongTensor_nDimension", &THLongTensor_nDimension);
  ctx.bind("THLongTensor_size", &THLongTensor_size);
  ctx.bind("THLongTensor_stride", &THLongTensor_stride);
  ctx.bind("THLongTensor_newSizeOf", &THLongTensor_newSizeOf);
  ctx.bind("THLongTensor_newStrideOf", &THLongTensor_newStrideOf);
  ctx.bind("THLongTensor_data", &THLongTensor_data);
  ctx.bind("THLongTensor_setFlag", &THLongTensor_setFlag);
  ctx.bind("THLongTensor_clearFlag", &THLongTensor_clearFlag);
  // Long Tensor allocation
  ctx.bind("THLongTensor_new", &THLongTensor_new);
  ctx.bind("THLongTensor_newWithTensor", &THLongTensor_newWithTensor);
  ctx.bind("THLongTensor_newWithStorage1d", &THLongTensor_newWithStorage1d);
  ctx.bind("THLongTensor_newWithStorage2d", &THLongTensor_newWithStorage2d);
  ctx.bind("THLongTensor_newWithStorage3d", &THLongTensor_newWithStorage3d);
  ctx.bind("THLongTensor_newWithStorage4d", &THLongTensor_newWithStorage4d);
  ctx.bind("THLongTensor_newWithSize1d", &THLongTensor_newWithSize1d);
  ctx.bind("THLongTensor_newWithSize2d", &THLongTensor_newWithSize2d);
  ctx.bind("THLongTensor_newWithSize3d", &THLongTensor_newWithSize3d);
  ctx.bind("THLongTensor_newWithSize4d", &THLongTensor_newWithSize4d);
  ctx.bind("THLongTensor_newClone", &THLongTensor_newClone);
  ctx.bind("THLongTensor_newContiguous", &THLongTensor_newContiguous);
  ctx.bind("THLongTensor_newSelect", &THLongTensor_newSelect);
  ctx.bind("THLongTensor_newNarrow", &THLongTensor_newNarrow);
  ctx.bind("THLongTensor_newTranspose", &THLongTensor_newTranspose);
  ctx.bind("THLongTensor_newUnfold", &THLongTensor_newUnfold);
  ctx.bind("THLongTensor_newView", &THLongTensor_newView);
  ctx.bind("THLongTensor_newExpand", &THLongTensor_newExpand);
  ctx.bind("THLongTensor_expand", &THLongTensor_expand);
  ctx.bind("THLongTensor_expandNd", &THLongTensor_expandNd);
  ctx.bind("THLongTensor_resize", &THLongTensor_resize);
  ctx.bind("THLongTensor_resizeAs", &THLongTensor_resizeAs);
  ctx.bind("THLongTensor_resize1d", &THLongTensor_resize1d);
  ctx.bind("THLongTensor_resize2d", &THLongTensor_resize2d);
  ctx.bind("THLongTensor_resize3d", &THLongTensor_resize3d);
  ctx.bind("THLongTensor_resize4d", &THLongTensor_resize4d);
  ctx.bind("THLongTensor_resize5d", &THLongTensor_resize5d);
  ctx.bind("THLongTensor_resizeNd", &THLongTensor_resizeNd);
  ctx.bind("THLongTensor_set", &THLongTensor_set);
  ctx.bind("THLongTensor_setStorage", &THLongTensor_setStorage);
  ctx.bind("THLongTensor_setStorageNd", &THLongTensor_setStorageNd);
  ctx.bind("THLongTensor_setStorage1d", &THLongTensor_setStorage1d);
  ctx.bind("THLongTensor_setStorage2d", &THLongTensor_setStorage2d);
  ctx.bind("THLongTensor_setStorage3d", &THLongTensor_setStorage3d);
  ctx.bind("THLongTensor_setStorage4d", &THLongTensor_setStorage4d);
  ctx.bind("THLongTensor_narrow", &THLongTensor_narrow);
  ctx.bind("THLongTensor_select", &THLongTensor_select);
  ctx.bind("THLongTensor_transpose", &THLongTensor_transpose);
  ctx.bind("THLongTensor_unfold", &THLongTensor_unfold);
  ctx.bind("THLongTensor_squeeze", &THLongTensor_squeeze);
  ctx.bind("THLongTensor_squeeze1d", &THLongTensor_squeeze1d);
  ctx.bind("THLongTensor_unsqueeze1d", &THLongTensor_unsqueeze1d);
  ctx.bind("THLongTensor_isContiguous", &THLongTensor_isContiguous);
  ctx.bind("THLongTensor_isSameSizeAs", &THLongTensor_isSameSizeAs);
  ctx.bind("THLongTensor_isSetTo", &THLongTensor_isSetTo);
  ctx.bind("THLongTensor_isSize", &THLongTensor_isSize);
  ctx.bind("THLongTensor_nElement", &THLongTensor_nElement);
  ctx.bind("THLongTensor_retain", &THLongTensor_retain);
  ctx.bind("THLongTensor_free", &THLongTensor_free);
  ctx.bind("THLongTensor_freeCopyTo", &THLongTensor_freeCopyTo);
  // Long Tensor slow access
  ctx.bind("THLongTensor_set1d", &THLongTensor_set1d);
  ctx.bind("THLongTensor_set2d", &THLongTensor_set2d);
  ctx.bind("THLongTensor_set3d", &THLongTensor_set3d);
  ctx.bind("THLongTensor_set4d", &THLongTensor_set4d);
  ctx.bind("THLongTensor_get1d", &THLongTensor_get1d);
  ctx.bind("THLongTensor_get2d", &THLongTensor_get2d);
  ctx.bind("THLongTensor_get3d", &THLongTensor_get3d);
  ctx.bind("THLongTensor_get4d", &THLongTensor_get4d);
  // Long Tensor Math
  ctx.bind("THLongTensor_fill", &THLongTensor_fill);
  ctx.bind("THLongTensor_zero", &THLongTensor_zero);
  ctx.bind("THLongTensor_maskedFill", &THLongTensor_maskedFill);
  ctx.bind("THLongTensor_maskedCopy", &THLongTensor_maskedCopy);
  ctx.bind("THLongTensor_maskedSelect", &THLongTensor_maskedSelect);
  ctx.bind("THLongTensor_nonzero", &THLongTensor_nonzero);
  ctx.bind("THLongTensor_indexSelect", &THLongTensor_indexSelect);
  ctx.bind("THLongTensor_indexCopy", &THLongTensor_indexCopy);
  ctx.bind("THLongTensor_indexAdd", &THLongTensor_indexAdd);
  ctx.bind("THLongTensor_indexFill", &THLongTensor_indexFill);
  ctx.bind("THLongTensor_gather", &THLongTensor_gather);
  ctx.bind("THLongTensor_scatter", &THLongTensor_scatter);
  ctx.bind("THLongTensor_scatterAdd", &THLongTensor_scatterAdd);
  ctx.bind("THLongTensor_scatterFill", &THLongTensor_scatterFill);
  ctx.bind("THLongTensor_dot", &THLongTensor_dot);
  ctx.bind("THLongTensor_minall", &THLongTensor_minall);
  ctx.bind("THLongTensor_maxall", &THLongTensor_maxall);
  ctx.bind("THLongTensor_medianall", &THLongTensor_medianall);
  ctx.bind("THLongTensor_sumall", &THLongTensor_sumall);
  ctx.bind("THLongTensor_prodall", &THLongTensor_prodall);
  ctx.bind("THLongTensor_add", &THLongTensor_add);
  ctx.bind("THLongTensor_sub", &THLongTensor_sub);
  ctx.bind("THLongTensor_mul", &THLongTensor_mul);
  ctx.bind("THLongTensor_div", &THLongTensor_div);
  ctx.bind("THLongTensor_lshift", &THLongTensor_lshift);
  ctx.bind("THLongTensor_rshift", &THLongTensor_rshift);
  ctx.bind("THLongTensor_fmod", &THLongTensor_fmod);
  ctx.bind("THLongTensor_remainder", &THLongTensor_remainder);
  ctx.bind("THLongTensor_clamp", &THLongTensor_clamp);
  ctx.bind("THLongTensor_bitand", &THLongTensor_bitand);
  ctx.bind("THLongTensor_bitor", &THLongTensor_bitor);
  ctx.bind("THLongTensor_bitxor", &THLongTensor_bitxor);
  ctx.bind("THLongTensor_cadd", &THLongTensor_cadd);
  ctx.bind("THLongTensor_csub", &THLongTensor_csub);
  ctx.bind("THLongTensor_cmul", &THLongTensor_cmul);
  ctx.bind("THLongTensor_cpow", &THLongTensor_cpow);
  ctx.bind("THLongTensor_cdiv", &THLongTensor_cdiv);
  ctx.bind("THLongTensor_clshift", &THLongTensor_clshift);
  ctx.bind("THLongTensor_crshift", &THLongTensor_crshift);
  ctx.bind("THLongTensor_cfmod", &THLongTensor_cfmod);
  ctx.bind("THLongTensor_cremainder", &THLongTensor_cremainder);
  ctx.bind("THLongTensor_cbitand", &THLongTensor_cbitand);
  ctx.bind("THLongTensor_cbitor", &THLongTensor_cbitor);
  ctx.bind("THLongTensor_cbitxor", &THLongTensor_cbitxor);
  ctx.bind("THLongTensor_addcmul", &THLongTensor_addcmul);
  ctx.bind("THLongTensor_addcdiv", &THLongTensor_addcdiv);
  ctx.bind("THLongTensor_addmv", &THLongTensor_addmv);
  ctx.bind("THLongTensor_addmm", &THLongTensor_addmm);
  ctx.bind("THLongTensor_addr", &THLongTensor_addr);
  ctx.bind("THLongTensor_addbmm", &THLongTensor_addbmm);
  ctx.bind("THLongTensor_baddbmm", &THLongTensor_baddbmm);
  ctx.bind("THLongTensor_match", &THLongTensor_match);
  ctx.bind("THLongTensor_numel", &THLongTensor_numel);
  ctx.bind("THLongTensor_max", &THLongTensor_max);
  ctx.bind("THLongTensor_min", &THLongTensor_min);
  ctx.bind("THLongTensor_kthvalue", &THLongTensor_kthvalue);
  ctx.bind("THLongTensor_mode", &THLongTensor_mode);
  ctx.bind("THLongTensor_median", &THLongTensor_median);
  ctx.bind("THLongTensor_sum", &THLongTensor_sum);
  ctx.bind("THLongTensor_prod", &THLongTensor_prod);
  ctx.bind("THLongTensor_cumsum", &THLongTensor_cumsum);
  ctx.bind("THLongTensor_cumprod", &THLongTensor_cumprod);
  ctx.bind("THLongTensor_sign", &THLongTensor_sign);
  ctx.bind("THLongTensor_trace", &THLongTensor_trace);
  ctx.bind("THLongTensor_cross", &THLongTensor_cross);
  ctx.bind("THLongTensor_cmax", &THLongTensor_cmax);
  ctx.bind("THLongTensor_cmin", &THLongTensor_cmin);
  ctx.bind("THLongTensor_cmaxValue", &THLongTensor_cmaxValue);
  ctx.bind("THLongTensor_cminValue", &THLongTensor_cminValue);
  ctx.bind("THLongTensor_zeros", &THLongTensor_zeros);
  ctx.bind("THLongTensor_zerosLike", &THLongTensor_zerosLike);
  ctx.bind("THLongTensor_ones", &THLongTensor_ones);
  ctx.bind("THLongTensor_onesLike", &THLongTensor_onesLike);
  ctx.bind("THLongTensor_diag", &THLongTensor_diag);
  ctx.bind("THLongTensor_eye", &THLongTensor_eye);
  ctx.bind("THLongTensor_arange", &THLongTensor_arange);
  ctx.bind("THLongTensor_range", &THLongTensor_range);
  ctx.bind("THLongTensor_randperm", &THLongTensor_randperm);
  ctx.bind("THLongTensor_reshape", &THLongTensor_reshape);
  ctx.bind("THLongTensor_sort", &THLongTensor_sort);
  ctx.bind("THLongTensor_topk", &THLongTensor_topk);
  ctx.bind("THLongTensor_tril", &THLongTensor_tril);
  ctx.bind("THLongTensor_triu", &THLongTensor_triu);
  ctx.bind("THLongTensor_cat", &THLongTensor_cat);
  ctx.bind("THLongTensor_catArray", &THLongTensor_catArray);
  ctx.bind("THLongTensor_equal", &THLongTensor_equal);
  ctx.bind("THLongTensor_ltValue", &THLongTensor_ltValue);
  ctx.bind("THLongTensor_leValue", &THLongTensor_leValue);
  ctx.bind("THLongTensor_gtValue", &THLongTensor_gtValue);
  ctx.bind("THLongTensor_geValue", &THLongTensor_geValue);
  ctx.bind("THLongTensor_neValue", &THLongTensor_neValue);
  ctx.bind("THLongTensor_eqValue", &THLongTensor_eqValue);
  ctx.bind("THLongTensor_ltValueT", &THLongTensor_ltValueT);
  ctx.bind("THLongTensor_leValueT", &THLongTensor_leValueT);
  ctx.bind("THLongTensor_gtValueT", &THLongTensor_gtValueT);
  ctx.bind("THLongTensor_geValueT", &THLongTensor_geValueT);
  ctx.bind("THLongTensor_neValueT", &THLongTensor_neValueT);
  ctx.bind("THLongTensor_eqValueT", &THLongTensor_eqValueT);
  ctx.bind("THLongTensor_ltTensor", &THLongTensor_ltTensor);
  ctx.bind("THLongTensor_leTensor", &THLongTensor_leTensor);
  ctx.bind("THLongTensor_gtTensor", &THLongTensor_gtTensor);
  ctx.bind("THLongTensor_geTensor", &THLongTensor_geTensor);
  ctx.bind("THLongTensor_neTensor", &THLongTensor_neTensor);
  ctx.bind("THLongTensor_eqTensor", &THLongTensor_eqTensor);
  ctx.bind("THLongTensor_ltTensorT", &THLongTensor_ltTensorT);
  ctx.bind("THLongTensor_leTensorT", &THLongTensor_leTensorT);
  ctx.bind("THLongTensor_gtTensorT", &THLongTensor_gtTensorT);
  ctx.bind("THLongTensor_geTensorT", &THLongTensor_geTensorT);
  ctx.bind("THLongTensor_neTensorT", &THLongTensor_neTensorT);
  ctx.bind("THLongTensor_eqTensorT", &THLongTensor_eqTensorT);
  // Long Random
  ctx.bind("THLongTensor_random", &THLongTensor_random);
  ctx.bind("THLongTensor_clampedRandom", &THLongTensor_clampedRandom);
  ctx.bind("THLongTensor_cappedRandom", &THLongTensor_cappedRandom);
  ctx.bind("THLongTensor_geometric", &THLongTensor_geometric);
  ctx.bind("THLongTensor_bernoulli", &THLongTensor_bernoulli);
  // Convolution
  ctx.bind("THLongTensor_validXCorr2Dptr", &THLongTensor_validXCorr2Dptr);
  ctx.bind("THLongTensor_validConv2Dptr", &THLongTensor_validConv2Dptr);
  ctx.bind("THLongTensor_fullXCorr2Dptr", &THLongTensor_fullXCorr2Dptr);
  ctx.bind("THLongTensor_fullConv2Dptr", &THLongTensor_fullConv2Dptr);
  ctx.bind("THLongTensor_validXCorr2DRevptr", &THLongTensor_validXCorr2DRevptr);
  ctx.bind("THLongTensor_conv2DRevger", &THLongTensor_conv2DRevger);
  ctx.bind("THLongTensor_conv2DRevgerm", &THLongTensor_conv2DRevgerm);
  ctx.bind("THLongTensor_conv2Dger", &THLongTensor_conv2Dger);
  ctx.bind("THLongTensor_conv2Dmv", &THLongTensor_conv2Dmv);
  ctx.bind("THLongTensor_conv2Dmm", &THLongTensor_conv2Dmm);
  ctx.bind("THLongTensor_conv2Dmul", &THLongTensor_conv2Dmul);
  ctx.bind("THLongTensor_conv2Dcmul", &THLongTensor_conv2Dcmul);
  ctx.bind("THLongTensor_validXCorr3Dptr", &THLongTensor_validXCorr3Dptr);
  ctx.bind("THLongTensor_validConv3Dptr", &THLongTensor_validConv3Dptr);
  ctx.bind("THLongTensor_fullXCorr3Dptr", &THLongTensor_fullXCorr3Dptr);
  ctx.bind("THLongTensor_fullConv3Dptr", &THLongTensor_fullConv3Dptr);
  ctx.bind("THLongTensor_validXCorr3DRevptr", &THLongTensor_validXCorr3DRevptr);
  ctx.bind("THLongTensor_conv3DRevger", &THLongTensor_conv3DRevger);
  ctx.bind("THLongTensor_conv3Dger", &THLongTensor_conv3Dger);
  ctx.bind("THLongTensor_conv3Dmv", &THLongTensor_conv3Dmv);
  ctx.bind("THLongTensor_conv3Dmul", &THLongTensor_conv3Dmul);
  ctx.bind("THLongTensor_conv3Dcmul", &THLongTensor_conv3Dcmul);

  // Int
  // Int Tensor access
  ctx.bind("THIntTensor_storage", &THIntTensor_storage);
  ctx.bind("THIntTensor_storageOffset", &THIntTensor_storageOffset);
  ctx.bind("THIntTensor_nDimension", &THIntTensor_nDimension);
  ctx.bind("THIntTensor_size", &THIntTensor_size);
  ctx.bind("THIntTensor_stride", &THIntTensor_stride);
  ctx.bind("THIntTensor_newSizeOf", &THIntTensor_newSizeOf);
  ctx.bind("THIntTensor_newStrideOf", &THIntTensor_newStrideOf);
  ctx.bind("THIntTensor_data", &THIntTensor_data);
  ctx.bind("THIntTensor_setFlag", &THIntTensor_setFlag);
  ctx.bind("THIntTensor_clearFlag", &THIntTensor_clearFlag);
  // Int Tensor allocation
  ctx.bind("THIntTensor_new", &THIntTensor_new);
  ctx.bind("THIntTensor_newWithTensor", &THIntTensor_newWithTensor);
  ctx.bind("THIntTensor_newWithStorage1d", &THIntTensor_newWithStorage1d);
  ctx.bind("THIntTensor_newWithStorage2d", &THIntTensor_newWithStorage2d);
  ctx.bind("THIntTensor_newWithStorage3d", &THIntTensor_newWithStorage3d);
  ctx.bind("THIntTensor_newWithStorage4d", &THIntTensor_newWithStorage4d);
  ctx.bind("THIntTensor_newWithSize1d", &THIntTensor_newWithSize1d);
  ctx.bind("THIntTensor_newWithSize2d", &THIntTensor_newWithSize2d);
  ctx.bind("THIntTensor_newWithSize3d", &THIntTensor_newWithSize3d);
  ctx.bind("THIntTensor_newWithSize4d", &THIntTensor_newWithSize4d);
  ctx.bind("THIntTensor_newClone", &THIntTensor_newClone);
  ctx.bind("THIntTensor_newContiguous", &THIntTensor_newContiguous);
  ctx.bind("THIntTensor_newSelect", &THIntTensor_newSelect);
  ctx.bind("THIntTensor_newNarrow", &THIntTensor_newNarrow);
  ctx.bind("THIntTensor_newTranspose", &THIntTensor_newTranspose);
  ctx.bind("THIntTensor_newUnfold", &THIntTensor_newUnfold);
  ctx.bind("THIntTensor_newView", &THIntTensor_newView);
  ctx.bind("THIntTensor_newExpand", &THIntTensor_newExpand);
  ctx.bind("THIntTensor_expand", &THIntTensor_expand);
  ctx.bind("THIntTensor_expandNd", &THIntTensor_expandNd);
  ctx.bind("THIntTensor_resize", &THIntTensor_resize);
  ctx.bind("THIntTensor_resizeAs", &THIntTensor_resizeAs);
  ctx.bind("THIntTensor_resize1d", &THIntTensor_resize1d);
  ctx.bind("THIntTensor_resize2d", &THIntTensor_resize2d);
  ctx.bind("THIntTensor_resize3d", &THIntTensor_resize3d);
  ctx.bind("THIntTensor_resize4d", &THIntTensor_resize4d);
  ctx.bind("THIntTensor_resize5d", &THIntTensor_resize5d);
  ctx.bind("THIntTensor_resizeNd", &THIntTensor_resizeNd);
  ctx.bind("THIntTensor_set", &THIntTensor_set);
  ctx.bind("THIntTensor_setStorage", &THIntTensor_setStorage);
  ctx.bind("THIntTensor_setStorageNd", &THIntTensor_setStorageNd);
  ctx.bind("THIntTensor_setStorage1d", &THIntTensor_setStorage1d);
  ctx.bind("THIntTensor_setStorage2d", &THIntTensor_setStorage2d);
  ctx.bind("THIntTensor_setStorage3d", &THIntTensor_setStorage3d);
  ctx.bind("THIntTensor_setStorage4d", &THIntTensor_setStorage4d);
  ctx.bind("THIntTensor_narrow", &THIntTensor_narrow);
  ctx.bind("THIntTensor_select", &THIntTensor_select);
  ctx.bind("THIntTensor_transpose", &THIntTensor_transpose);
  ctx.bind("THIntTensor_unfold", &THIntTensor_unfold);
  ctx.bind("THIntTensor_squeeze", &THIntTensor_squeeze);
  ctx.bind("THIntTensor_squeeze1d", &THIntTensor_squeeze1d);
  ctx.bind("THIntTensor_unsqueeze1d", &THIntTensor_unsqueeze1d);
  ctx.bind("THIntTensor_isContiguous", &THIntTensor_isContiguous);
  ctx.bind("THIntTensor_isSameSizeAs", &THIntTensor_isSameSizeAs);
  ctx.bind("THIntTensor_isSetTo", &THIntTensor_isSetTo);
  ctx.bind("THIntTensor_isSize", &THIntTensor_isSize);
  ctx.bind("THIntTensor_nElement", &THIntTensor_nElement);
  ctx.bind("THIntTensor_retain", &THIntTensor_retain);
  ctx.bind("THIntTensor_free", &THIntTensor_free);
  ctx.bind("THIntTensor_freeCopyTo", &THIntTensor_freeCopyTo);
  // Int Tensor slow access
  ctx.bind("THIntTensor_set1d", &THIntTensor_set1d);
  ctx.bind("THIntTensor_set2d", &THIntTensor_set2d);
  ctx.bind("THIntTensor_set3d", &THIntTensor_set3d);
  ctx.bind("THIntTensor_set4d", &THIntTensor_set4d);
  ctx.bind("THIntTensor_get1d", &THIntTensor_get1d);
  ctx.bind("THIntTensor_get2d", &THIntTensor_get2d);
  ctx.bind("THIntTensor_get3d", &THIntTensor_get3d);
  ctx.bind("THIntTensor_get4d", &THIntTensor_get4d);
  // Int Tensor Math
  ctx.bind("THIntTensor_fill", &THIntTensor_fill);
  ctx.bind("THIntTensor_zero", &THIntTensor_zero);
  ctx.bind("THIntTensor_maskedFill", &THIntTensor_maskedFill);
  ctx.bind("THIntTensor_maskedCopy", &THIntTensor_maskedCopy);
  ctx.bind("THIntTensor_maskedSelect", &THIntTensor_maskedSelect);
  ctx.bind("THIntTensor_nonzero", &THIntTensor_nonzero);
  ctx.bind("THIntTensor_indexSelect", &THIntTensor_indexSelect);
  ctx.bind("THIntTensor_indexCopy", &THIntTensor_indexCopy);
  ctx.bind("THIntTensor_indexAdd", &THIntTensor_indexAdd);
  ctx.bind("THIntTensor_indexFill", &THIntTensor_indexFill);
  ctx.bind("THIntTensor_gather", &THIntTensor_gather);
  ctx.bind("THIntTensor_scatter", &THIntTensor_scatter);
  ctx.bind("THIntTensor_scatterAdd", &THIntTensor_scatterAdd);
  ctx.bind("THIntTensor_scatterFill", &THIntTensor_scatterFill);
  ctx.bind("THIntTensor_dot", &THIntTensor_dot);
  ctx.bind("THIntTensor_minall", &THIntTensor_minall);
  ctx.bind("THIntTensor_maxall", &THIntTensor_maxall);
  ctx.bind("THIntTensor_medianall", &THIntTensor_medianall);
  ctx.bind("THIntTensor_sumall", &THIntTensor_sumall);
  ctx.bind("THIntTensor_prodall", &THIntTensor_prodall);
  ctx.bind("THIntTensor_add", &THIntTensor_add);
  ctx.bind("THIntTensor_sub", &THIntTensor_sub);
  ctx.bind("THIntTensor_mul", &THIntTensor_mul);
  ctx.bind("THIntTensor_div", &THIntTensor_div);
  ctx.bind("THIntTensor_lshift", &THIntTensor_lshift);
  ctx.bind("THIntTensor_rshift", &THIntTensor_rshift);
  ctx.bind("THIntTensor_fmod", &THIntTensor_fmod);
  ctx.bind("THIntTensor_remainder", &THIntTensor_remainder);
  ctx.bind("THIntTensor_clamp", &THIntTensor_clamp);
  ctx.bind("THIntTensor_bitand", &THIntTensor_bitand);
  ctx.bind("THIntTensor_bitor", &THIntTensor_bitor);
  ctx.bind("THIntTensor_bitxor", &THIntTensor_bitxor);
  ctx.bind("THIntTensor_cadd", &THIntTensor_cadd);
  ctx.bind("THIntTensor_csub", &THIntTensor_csub);
  ctx.bind("THIntTensor_cmul", &THIntTensor_cmul);
  ctx.bind("THIntTensor_cpow", &THIntTensor_cpow);
  ctx.bind("THIntTensor_cdiv", &THIntTensor_cdiv);
  ctx.bind("THIntTensor_clshift", &THIntTensor_clshift);
  ctx.bind("THIntTensor_crshift", &THIntTensor_crshift);
  ctx.bind("THIntTensor_cfmod", &THIntTensor_cfmod);
  ctx.bind("THIntTensor_cremainder", &THIntTensor_cremainder);
  ctx.bind("THIntTensor_cbitand", &THIntTensor_cbitand);
  ctx.bind("THIntTensor_cbitor", &THIntTensor_cbitor);
  ctx.bind("THIntTensor_cbitxor", &THIntTensor_cbitxor);
  ctx.bind("THIntTensor_addcmul", &THIntTensor_addcmul);
  ctx.bind("THIntTensor_addcdiv", &THIntTensor_addcdiv);
  ctx.bind("THIntTensor_addmv", &THIntTensor_addmv);
  ctx.bind("THIntTensor_addmm", &THIntTensor_addmm);
  ctx.bind("THIntTensor_addr", &THIntTensor_addr);
  ctx.bind("THIntTensor_addbmm", &THIntTensor_addbmm);
  ctx.bind("THIntTensor_baddbmm", &THIntTensor_baddbmm);
  ctx.bind("THIntTensor_match", &THIntTensor_match);
  ctx.bind("THIntTensor_numel", &THIntTensor_numel);
  ctx.bind("THIntTensor_max", &THIntTensor_max);
  ctx.bind("THIntTensor_min", &THIntTensor_min);
  ctx.bind("THIntTensor_kthvalue", &THIntTensor_kthvalue);
  ctx.bind("THIntTensor_mode", &THIntTensor_mode);
  ctx.bind("THIntTensor_median", &THIntTensor_median);
  ctx.bind("THIntTensor_sum", &THIntTensor_sum);
  ctx.bind("THIntTensor_prod", &THIntTensor_prod);
  ctx.bind("THIntTensor_cumsum", &THIntTensor_cumsum);
  ctx.bind("THIntTensor_cumprod", &THIntTensor_cumprod);
  ctx.bind("THIntTensor_sign", &THIntTensor_sign);
  ctx.bind("THIntTensor_trace", &THIntTensor_trace);
  ctx.bind("THIntTensor_cross", &THIntTensor_cross);
  ctx.bind("THIntTensor_cmax", &THIntTensor_cmax);
  ctx.bind("THIntTensor_cmin", &THIntTensor_cmin);
  ctx.bind("THIntTensor_cmaxValue", &THIntTensor_cmaxValue);
  ctx.bind("THIntTensor_cminValue", &THIntTensor_cminValue);
  ctx.bind("THIntTensor_zeros", &THIntTensor_zeros);
  ctx.bind("THIntTensor_zerosLike", &THIntTensor_zerosLike);
  ctx.bind("THIntTensor_ones", &THIntTensor_ones);
  ctx.bind("THIntTensor_onesLike", &THIntTensor_onesLike);
  ctx.bind("THIntTensor_diag", &THIntTensor_diag);
  ctx.bind("THIntTensor_eye", &THIntTensor_eye);
  ctx.bind("THIntTensor_arange", &THIntTensor_arange);
  ctx.bind("THIntTensor_range", &THIntTensor_range);
  ctx.bind("THIntTensor_randperm", &THIntTensor_randperm);
  ctx.bind("THIntTensor_reshape", &THIntTensor_reshape);
  ctx.bind("THIntTensor_sort", &THIntTensor_sort);
  ctx.bind("THIntTensor_topk", &THIntTensor_topk);
  ctx.bind("THIntTensor_tril", &THIntTensor_tril);
  ctx.bind("THIntTensor_triu", &THIntTensor_triu);
  ctx.bind("THIntTensor_cat", &THIntTensor_cat);
  ctx.bind("THIntTensor_catArray", &THIntTensor_catArray);
  ctx.bind("THIntTensor_equal", &THIntTensor_equal);
  ctx.bind("THIntTensor_ltValue", &THIntTensor_ltValue);
  ctx.bind("THIntTensor_leValue", &THIntTensor_leValue);
  ctx.bind("THIntTensor_gtValue", &THIntTensor_gtValue);
  ctx.bind("THIntTensor_geValue", &THIntTensor_geValue);
  ctx.bind("THIntTensor_neValue", &THIntTensor_neValue);
  ctx.bind("THIntTensor_eqValue", &THIntTensor_eqValue);
  ctx.bind("THIntTensor_ltValueT", &THIntTensor_ltValueT);
  ctx.bind("THIntTensor_leValueT", &THIntTensor_leValueT);
  ctx.bind("THIntTensor_gtValueT", &THIntTensor_gtValueT);
  ctx.bind("THIntTensor_geValueT", &THIntTensor_geValueT);
  ctx.bind("THIntTensor_neValueT", &THIntTensor_neValueT);
  ctx.bind("THIntTensor_eqValueT", &THIntTensor_eqValueT);
  ctx.bind("THIntTensor_ltTensor", &THIntTensor_ltTensor);
  ctx.bind("THIntTensor_leTensor", &THIntTensor_leTensor);
  ctx.bind("THIntTensor_gtTensor", &THIntTensor_gtTensor);
  ctx.bind("THIntTensor_geTensor", &THIntTensor_geTensor);
  ctx.bind("THIntTensor_neTensor", &THIntTensor_neTensor);
  ctx.bind("THIntTensor_eqTensor", &THIntTensor_eqTensor);
  ctx.bind("THIntTensor_ltTensorT", &THIntTensor_ltTensorT);
  ctx.bind("THIntTensor_leTensorT", &THIntTensor_leTensorT);
  ctx.bind("THIntTensor_gtTensorT", &THIntTensor_gtTensorT);
  ctx.bind("THIntTensor_geTensorT", &THIntTensor_geTensorT);
  ctx.bind("THIntTensor_neTensorT", &THIntTensor_neTensorT);
  ctx.bind("THIntTensor_eqTensorT", &THIntTensor_eqTensorT);
  // Int Random
  ctx.bind("THIntTensor_random", &THIntTensor_random);
  ctx.bind("THIntTensor_clampedRandom", &THIntTensor_clampedRandom);
  ctx.bind("THIntTensor_cappedRandom", &THIntTensor_cappedRandom);
  ctx.bind("THIntTensor_geometric", &THIntTensor_geometric);
  ctx.bind("THIntTensor_bernoulli", &THIntTensor_bernoulli);
  // Convolution
  ctx.bind("THIntTensor_validXCorr2Dptr", &THIntTensor_validXCorr2Dptr);
  ctx.bind("THIntTensor_validConv2Dptr", &THIntTensor_validConv2Dptr);
  ctx.bind("THIntTensor_fullXCorr2Dptr", &THIntTensor_fullXCorr2Dptr);
  ctx.bind("THIntTensor_fullConv2Dptr", &THIntTensor_fullConv2Dptr);
  ctx.bind("THIntTensor_validXCorr2DRevptr", &THIntTensor_validXCorr2DRevptr);
  ctx.bind("THIntTensor_conv2DRevger", &THIntTensor_conv2DRevger);
  ctx.bind("THIntTensor_conv2DRevgerm", &THIntTensor_conv2DRevgerm);
  ctx.bind("THIntTensor_conv2Dger", &THIntTensor_conv2Dger);
  ctx.bind("THIntTensor_conv2Dmv", &THIntTensor_conv2Dmv);
  ctx.bind("THIntTensor_conv2Dmm", &THIntTensor_conv2Dmm);
  ctx.bind("THIntTensor_conv2Dmul", &THIntTensor_conv2Dmul);
  ctx.bind("THIntTensor_conv2Dcmul", &THIntTensor_conv2Dcmul);
  ctx.bind("THIntTensor_validXCorr3Dptr", &THIntTensor_validXCorr3Dptr);
  ctx.bind("THIntTensor_validConv3Dptr", &THIntTensor_validConv3Dptr);
  ctx.bind("THIntTensor_fullXCorr3Dptr", &THIntTensor_fullXCorr3Dptr);
  ctx.bind("THIntTensor_fullConv3Dptr", &THIntTensor_fullConv3Dptr);
  ctx.bind("THIntTensor_validXCorr3DRevptr", &THIntTensor_validXCorr3DRevptr);
  ctx.bind("THIntTensor_conv3DRevger", &THIntTensor_conv3DRevger);
  ctx.bind("THIntTensor_conv3Dger", &THIntTensor_conv3Dger);
  ctx.bind("THIntTensor_conv3Dmv", &THIntTensor_conv3Dmv);
  ctx.bind("THIntTensor_conv3Dmul", &THIntTensor_conv3Dmul);
  ctx.bind("THIntTensor_conv3Dcmul", &THIntTensor_conv3Dcmul);

  // Short
  // Short Tensor access
  ctx.bind("THShortTensor_storage", &THShortTensor_storage);
  ctx.bind("THShortTensor_storageOffset", &THShortTensor_storageOffset);
  ctx.bind("THShortTensor_nDimension", &THShortTensor_nDimension);
  ctx.bind("THShortTensor_size", &THShortTensor_size);
  ctx.bind("THShortTensor_stride", &THShortTensor_stride);
  ctx.bind("THShortTensor_newSizeOf", &THShortTensor_newSizeOf);
  ctx.bind("THShortTensor_newStrideOf", &THShortTensor_newStrideOf);
  ctx.bind("THShortTensor_data", &THShortTensor_data);
  ctx.bind("THShortTensor_setFlag", &THShortTensor_setFlag);
  ctx.bind("THShortTensor_clearFlag", &THShortTensor_clearFlag);
  // Short Tensor allocation
  ctx.bind("THShortTensor_new", &THShortTensor_new);
  ctx.bind("THShortTensor_newWithTensor", &THShortTensor_newWithTensor);
  ctx.bind("THShortTensor_newWithStorage1d", &THShortTensor_newWithStorage1d);
  ctx.bind("THShortTensor_newWithStorage2d", &THShortTensor_newWithStorage2d);
  ctx.bind("THShortTensor_newWithStorage3d", &THShortTensor_newWithStorage3d);
  ctx.bind("THShortTensor_newWithStorage4d", &THShortTensor_newWithStorage4d);
  ctx.bind("THShortTensor_newWithSize1d", &THShortTensor_newWithSize1d);
  ctx.bind("THShortTensor_newWithSize2d", &THShortTensor_newWithSize2d);
  ctx.bind("THShortTensor_newWithSize3d", &THShortTensor_newWithSize3d);
  ctx.bind("THShortTensor_newWithSize4d", &THShortTensor_newWithSize4d);
  ctx.bind("THShortTensor_newClone", &THShortTensor_newClone);
  ctx.bind("THShortTensor_newContiguous", &THShortTensor_newContiguous);
  ctx.bind("THShortTensor_newSelect", &THShortTensor_newSelect);
  ctx.bind("THShortTensor_newNarrow", &THShortTensor_newNarrow);
  ctx.bind("THShortTensor_newTranspose", &THShortTensor_newTranspose);
  ctx.bind("THShortTensor_newUnfold", &THShortTensor_newUnfold);
  ctx.bind("THShortTensor_newView", &THShortTensor_newView);
  ctx.bind("THShortTensor_newExpand", &THShortTensor_newExpand);
  ctx.bind("THShortTensor_expand", &THShortTensor_expand);
  ctx.bind("THShortTensor_expandNd", &THShortTensor_expandNd);
  ctx.bind("THShortTensor_resize", &THShortTensor_resize);
  ctx.bind("THShortTensor_resizeAs", &THShortTensor_resizeAs);
  ctx.bind("THShortTensor_resize1d", &THShortTensor_resize1d);
  ctx.bind("THShortTensor_resize2d", &THShortTensor_resize2d);
  ctx.bind("THShortTensor_resize3d", &THShortTensor_resize3d);
  ctx.bind("THShortTensor_resize4d", &THShortTensor_resize4d);
  ctx.bind("THShortTensor_resize5d", &THShortTensor_resize5d);
  ctx.bind("THShortTensor_resizeNd", &THShortTensor_resizeNd);
  ctx.bind("THShortTensor_set", &THShortTensor_set);
  ctx.bind("THShortTensor_setStorage", &THShortTensor_setStorage);
  ctx.bind("THShortTensor_setStorageNd", &THShortTensor_setStorageNd);
  ctx.bind("THShortTensor_setStorage1d", &THShortTensor_setStorage1d);
  ctx.bind("THShortTensor_setStorage2d", &THShortTensor_setStorage2d);
  ctx.bind("THShortTensor_setStorage3d", &THShortTensor_setStorage3d);
  ctx.bind("THShortTensor_setStorage4d", &THShortTensor_setStorage4d);
  ctx.bind("THShortTensor_narrow", &THShortTensor_narrow);
  ctx.bind("THShortTensor_select", &THShortTensor_select);
  ctx.bind("THShortTensor_transpose", &THShortTensor_transpose);
  ctx.bind("THShortTensor_unfold", &THShortTensor_unfold);
  ctx.bind("THShortTensor_squeeze", &THShortTensor_squeeze);
  ctx.bind("THShortTensor_squeeze1d", &THShortTensor_squeeze1d);
  ctx.bind("THShortTensor_unsqueeze1d", &THShortTensor_unsqueeze1d);
  ctx.bind("THShortTensor_isContiguous", &THShortTensor_isContiguous);
  ctx.bind("THShortTensor_isSameSizeAs", &THShortTensor_isSameSizeAs);
  ctx.bind("THShortTensor_isSetTo", &THShortTensor_isSetTo);
  ctx.bind("THShortTensor_isSize", &THShortTensor_isSize);
  ctx.bind("THShortTensor_nElement", &THShortTensor_nElement);
  ctx.bind("THShortTensor_retain", &THShortTensor_retain);
  ctx.bind("THShortTensor_free", &THShortTensor_free);
  ctx.bind("THShortTensor_freeCopyTo", &THShortTensor_freeCopyTo);
  // Short Tensor slow access
  ctx.bind("THShortTensor_set1d", &THShortTensor_set1d);
  ctx.bind("THShortTensor_set2d", &THShortTensor_set2d);
  ctx.bind("THShortTensor_set3d", &THShortTensor_set3d);
  ctx.bind("THShortTensor_set4d", &THShortTensor_set4d);
  ctx.bind("THShortTensor_get1d", &THShortTensor_get1d);
  ctx.bind("THShortTensor_get2d", &THShortTensor_get2d);
  ctx.bind("THShortTensor_get3d", &THShortTensor_get3d);
  ctx.bind("THShortTensor_get4d", &THShortTensor_get4d);
  // Short Tensor Math
  ctx.bind("THShortTensor_fill", &THShortTensor_fill);
  ctx.bind("THShortTensor_zero", &THShortTensor_zero);
  ctx.bind("THShortTensor_maskedFill", &THShortTensor_maskedFill);
  ctx.bind("THShortTensor_maskedCopy", &THShortTensor_maskedCopy);
  ctx.bind("THShortTensor_maskedSelect", &THShortTensor_maskedSelect);
  ctx.bind("THShortTensor_nonzero", &THShortTensor_nonzero);
  ctx.bind("THShortTensor_indexSelect", &THShortTensor_indexSelect);
  ctx.bind("THShortTensor_indexCopy", &THShortTensor_indexCopy);
  ctx.bind("THShortTensor_indexAdd", &THShortTensor_indexAdd);
  ctx.bind("THShortTensor_indexFill", &THShortTensor_indexFill);
  ctx.bind("THShortTensor_gather", &THShortTensor_gather);
  ctx.bind("THShortTensor_scatter", &THShortTensor_scatter);
  ctx.bind("THShortTensor_scatterAdd", &THShortTensor_scatterAdd);
  ctx.bind("THShortTensor_scatterFill", &THShortTensor_scatterFill);
  ctx.bind("THShortTensor_dot", &THShortTensor_dot);
  ctx.bind("THShortTensor_minall", &THShortTensor_minall);
  ctx.bind("THShortTensor_maxall", &THShortTensor_maxall);
  ctx.bind("THShortTensor_medianall", &THShortTensor_medianall);
  ctx.bind("THShortTensor_sumall", &THShortTensor_sumall);
  ctx.bind("THShortTensor_prodall", &THShortTensor_prodall);
  ctx.bind("THShortTensor_neg", &THShortTensor_neg);
  ctx.bind("THShortTensor_add", &THShortTensor_add);
  ctx.bind("THShortTensor_sub", &THShortTensor_sub);
  ctx.bind("THShortTensor_mul", &THShortTensor_mul);
  ctx.bind("THShortTensor_div", &THShortTensor_div);
  ctx.bind("THShortTensor_lshift", &THShortTensor_lshift);
  ctx.bind("THShortTensor_rshift", &THShortTensor_rshift);
  ctx.bind("THShortTensor_fmod", &THShortTensor_fmod);
  ctx.bind("THShortTensor_remainder", &THShortTensor_remainder);
  ctx.bind("THShortTensor_clamp", &THShortTensor_clamp);
  ctx.bind("THShortTensor_bitand", &THShortTensor_bitand);
  ctx.bind("THShortTensor_bitor", &THShortTensor_bitor);
  ctx.bind("THShortTensor_bitxor", &THShortTensor_bitxor);
  ctx.bind("THShortTensor_cadd", &THShortTensor_cadd);
  ctx.bind("THShortTensor_csub", &THShortTensor_csub);
  ctx.bind("THShortTensor_cmul", &THShortTensor_cmul);
  ctx.bind("THShortTensor_cpow", &THShortTensor_cpow);
  ctx.bind("THShortTensor_cdiv", &THShortTensor_cdiv);
  ctx.bind("THShortTensor_clshift", &THShortTensor_clshift);
  ctx.bind("THShortTensor_crshift", &THShortTensor_crshift);
  ctx.bind("THShortTensor_cfmod", &THShortTensor_cfmod);
  ctx.bind("THShortTensor_cremainder", &THShortTensor_cremainder);
  ctx.bind("THShortTensor_cbitand", &THShortTensor_cbitand);
  ctx.bind("THShortTensor_cbitor", &THShortTensor_cbitor);
  ctx.bind("THShortTensor_cbitxor", &THShortTensor_cbitxor);
  ctx.bind("THShortTensor_addcmul", &THShortTensor_addcmul);
  ctx.bind("THShortTensor_addcdiv", &THShortTensor_addcdiv);
  ctx.bind("THShortTensor_addmv", &THShortTensor_addmv);
  ctx.bind("THShortTensor_addmm", &THShortTensor_addmm);
  ctx.bind("THShortTensor_addr", &THShortTensor_addr);
  ctx.bind("THShortTensor_addbmm", &THShortTensor_addbmm);
  ctx.bind("THShortTensor_baddbmm", &THShortTensor_baddbmm);
  ctx.bind("THShortTensor_match", &THShortTensor_match);
  ctx.bind("THShortTensor_numel", &THShortTensor_numel);
  ctx.bind("THShortTensor_max", &THShortTensor_max);
  ctx.bind("THShortTensor_min", &THShortTensor_min);
  ctx.bind("THShortTensor_kthvalue", &THShortTensor_kthvalue);
  ctx.bind("THShortTensor_mode", &THShortTensor_mode);
  ctx.bind("THShortTensor_median", &THShortTensor_median);
  ctx.bind("THShortTensor_sum", &THShortTensor_sum);
  ctx.bind("THShortTensor_prod", &THShortTensor_prod);
  ctx.bind("THShortTensor_cumsum", &THShortTensor_cumsum);
  ctx.bind("THShortTensor_cumprod", &THShortTensor_cumprod);
  ctx.bind("THShortTensor_sign", &THShortTensor_sign);
  ctx.bind("THShortTensor_trace", &THShortTensor_trace);
  ctx.bind("THShortTensor_cross", &THShortTensor_cross);
  ctx.bind("THShortTensor_cmax", &THShortTensor_cmax);
  ctx.bind("THShortTensor_cmin", &THShortTensor_cmin);
  ctx.bind("THShortTensor_cmaxValue", &THShortTensor_cmaxValue);
  ctx.bind("THShortTensor_cminValue", &THShortTensor_cminValue);
  ctx.bind("THShortTensor_zeros", &THShortTensor_zeros);
  ctx.bind("THShortTensor_zerosLike", &THShortTensor_zerosLike);
  ctx.bind("THShortTensor_ones", &THShortTensor_ones);
  ctx.bind("THShortTensor_onesLike", &THShortTensor_onesLike);
  ctx.bind("THShortTensor_diag", &THShortTensor_diag);
  ctx.bind("THShortTensor_eye", &THShortTensor_eye);
  ctx.bind("THShortTensor_arange", &THShortTensor_arange);
  ctx.bind("THShortTensor_range", &THShortTensor_range);
  ctx.bind("THShortTensor_randperm", &THShortTensor_randperm);
  ctx.bind("THShortTensor_reshape", &THShortTensor_reshape);
  ctx.bind("THShortTensor_sort", &THShortTensor_sort);
  ctx.bind("THShortTensor_topk", &THShortTensor_topk);
  ctx.bind("THShortTensor_tril", &THShortTensor_tril);
  ctx.bind("THShortTensor_triu", &THShortTensor_triu);
  ctx.bind("THShortTensor_cat", &THShortTensor_cat);
  ctx.bind("THShortTensor_catArray", &THShortTensor_catArray);
  ctx.bind("THShortTensor_equal", &THShortTensor_equal);
  ctx.bind("THShortTensor_ltValue", &THShortTensor_ltValue);
  ctx.bind("THShortTensor_leValue", &THShortTensor_leValue);
  ctx.bind("THShortTensor_gtValue", &THShortTensor_gtValue);
  ctx.bind("THShortTensor_geValue", &THShortTensor_geValue);
  ctx.bind("THShortTensor_neValue", &THShortTensor_neValue);
  ctx.bind("THShortTensor_eqValue", &THShortTensor_eqValue);
  ctx.bind("THShortTensor_ltValueT", &THShortTensor_ltValueT);
  ctx.bind("THShortTensor_leValueT", &THShortTensor_leValueT);
  ctx.bind("THShortTensor_gtValueT", &THShortTensor_gtValueT);
  ctx.bind("THShortTensor_geValueT", &THShortTensor_geValueT);
  ctx.bind("THShortTensor_neValueT", &THShortTensor_neValueT);
  ctx.bind("THShortTensor_eqValueT", &THShortTensor_eqValueT);
  ctx.bind("THShortTensor_ltTensor", &THShortTensor_ltTensor);
  ctx.bind("THShortTensor_leTensor", &THShortTensor_leTensor);
  ctx.bind("THShortTensor_gtTensor", &THShortTensor_gtTensor);
  ctx.bind("THShortTensor_geTensor", &THShortTensor_geTensor);
  ctx.bind("THShortTensor_neTensor", &THShortTensor_neTensor);
  ctx.bind("THShortTensor_eqTensor", &THShortTensor_eqTensor);
  ctx.bind("THShortTensor_ltTensorT", &THShortTensor_ltTensorT);
  ctx.bind("THShortTensor_leTensorT", &THShortTensor_leTensorT);
  ctx.bind("THShortTensor_gtTensorT", &THShortTensor_gtTensorT);
  ctx.bind("THShortTensor_geTensorT", &THShortTensor_geTensorT);
  ctx.bind("THShortTensor_neTensorT", &THShortTensor_neTensorT);
  ctx.bind("THShortTensor_eqTensorT", &THShortTensor_eqTensorT);
  // Short Random
  ctx.bind("THShortTensor_random", &THShortTensor_random);
  ctx.bind("THShortTensor_clampedRandom", &THShortTensor_clampedRandom);
  ctx.bind("THShortTensor_cappedRandom", &THShortTensor_cappedRandom);
  ctx.bind("THShortTensor_geometric", &THShortTensor_geometric);
  ctx.bind("THShortTensor_bernoulli", &THShortTensor_bernoulli);
  // Convolution
  ctx.bind("THShortTensor_validXCorr2Dptr", &THShortTensor_validXCorr2Dptr);
  ctx.bind("THShortTensor_validConv2Dptr", &THShortTensor_validConv2Dptr);
  ctx.bind("THShortTensor_fullXCorr2Dptr", &THShortTensor_fullXCorr2Dptr);
  ctx.bind("THShortTensor_fullConv2Dptr", &THShortTensor_fullConv2Dptr);
  ctx.bind("THShortTensor_validXCorr2DRevptr", &THShortTensor_validXCorr2DRevptr);
  ctx.bind("THShortTensor_conv2DRevger", &THShortTensor_conv2DRevger);
  ctx.bind("THShortTensor_conv2DRevgerm", &THShortTensor_conv2DRevgerm);
  ctx.bind("THShortTensor_conv2Dger", &THShortTensor_conv2Dger);
  ctx.bind("THShortTensor_conv2Dmv", &THShortTensor_conv2Dmv);
  ctx.bind("THShortTensor_conv2Dmm", &THShortTensor_conv2Dmm);
  ctx.bind("THShortTensor_conv2Dmul", &THShortTensor_conv2Dmul);
  ctx.bind("THShortTensor_conv2Dcmul", &THShortTensor_conv2Dcmul);
  ctx.bind("THShortTensor_validXCorr3Dptr", &THShortTensor_validXCorr3Dptr);
  ctx.bind("THShortTensor_validConv3Dptr", &THShortTensor_validConv3Dptr);
  ctx.bind("THShortTensor_fullXCorr3Dptr", &THShortTensor_fullXCorr3Dptr);
  ctx.bind("THShortTensor_fullConv3Dptr", &THShortTensor_fullConv3Dptr);
  ctx.bind("THShortTensor_validXCorr3DRevptr", &THShortTensor_validXCorr3DRevptr);
  ctx.bind("THShortTensor_conv3DRevger", &THShortTensor_conv3DRevger);
  ctx.bind("THShortTensor_conv3Dger", &THShortTensor_conv3Dger);
  ctx.bind("THShortTensor_conv3Dmv", &THShortTensor_conv3Dmv);
  ctx.bind("THShortTensor_conv3Dmul", &THShortTensor_conv3Dmul);
  ctx.bind("THShortTensor_conv3Dcmul", &THShortTensor_conv3Dcmul);
  
  // Byte
  // Byte Tensor access
  ctx.bind("THByteTensor_storage", &THByteTensor_storage);
  ctx.bind("THByteTensor_storageOffset", &THByteTensor_storageOffset);
  ctx.bind("THByteTensor_nDimension", &THByteTensor_nDimension);
  ctx.bind("THByteTensor_size", &THByteTensor_size);
  ctx.bind("THByteTensor_stride", &THByteTensor_stride);
  ctx.bind("THByteTensor_newSizeOf", &THByteTensor_newSizeOf);
  ctx.bind("THByteTensor_newStrideOf", &THByteTensor_newStrideOf);
  ctx.bind("THByteTensor_data", &THByteTensor_data);
  ctx.bind("THByteTensor_setFlag", &THByteTensor_setFlag);
  ctx.bind("THByteTensor_clearFlag", &THByteTensor_clearFlag);
  // Byte Tensor allocation
  ctx.bind("THByteTensor_new", &THByteTensor_new);
  ctx.bind("THByteTensor_newWithTensor", &THByteTensor_newWithTensor);
  ctx.bind("THByteTensor_newWithStorage1d", &THByteTensor_newWithStorage1d);
  ctx.bind("THByteTensor_newWithStorage2d", &THByteTensor_newWithStorage2d);
  ctx.bind("THByteTensor_newWithStorage3d", &THByteTensor_newWithStorage3d);
  ctx.bind("THByteTensor_newWithStorage4d", &THByteTensor_newWithStorage4d);
  ctx.bind("THByteTensor_newWithSize1d", &THByteTensor_newWithSize1d);
  ctx.bind("THByteTensor_newWithSize2d", &THByteTensor_newWithSize2d);
  ctx.bind("THByteTensor_newWithSize3d", &THByteTensor_newWithSize3d);
  ctx.bind("THByteTensor_newWithSize4d", &THByteTensor_newWithSize4d);
  ctx.bind("THByteTensor_newClone", &THByteTensor_newClone);
  ctx.bind("THByteTensor_newContiguous", &THByteTensor_newContiguous);
  ctx.bind("THByteTensor_newSelect", &THByteTensor_newSelect);
  ctx.bind("THByteTensor_newNarrow", &THByteTensor_newNarrow);
  ctx.bind("THByteTensor_newTranspose", &THByteTensor_newTranspose);
  ctx.bind("THByteTensor_newUnfold", &THByteTensor_newUnfold);
  ctx.bind("THByteTensor_newView", &THByteTensor_newView);
  ctx.bind("THByteTensor_newExpand", &THByteTensor_newExpand);
  ctx.bind("THByteTensor_expand", &THByteTensor_expand);
  ctx.bind("THByteTensor_expandNd", &THByteTensor_expandNd);
  ctx.bind("THByteTensor_resize", &THByteTensor_resize);
  ctx.bind("THByteTensor_resizeAs", &THByteTensor_resizeAs);
  ctx.bind("THByteTensor_resize1d", &THByteTensor_resize1d);
  ctx.bind("THByteTensor_resize2d", &THByteTensor_resize2d);
  ctx.bind("THByteTensor_resize3d", &THByteTensor_resize3d);
  ctx.bind("THByteTensor_resize4d", &THByteTensor_resize4d);
  ctx.bind("THByteTensor_resize5d", &THByteTensor_resize5d);
  ctx.bind("THByteTensor_resizeNd", &THByteTensor_resizeNd);
  ctx.bind("THByteTensor_set", &THByteTensor_set);
  ctx.bind("THByteTensor_setStorage", &THByteTensor_setStorage);
  ctx.bind("THByteTensor_setStorageNd", &THByteTensor_setStorageNd);
  ctx.bind("THByteTensor_setStorage1d", &THByteTensor_setStorage1d);
  ctx.bind("THByteTensor_setStorage2d", &THByteTensor_setStorage2d);
  ctx.bind("THByteTensor_setStorage3d", &THByteTensor_setStorage3d);
  ctx.bind("THByteTensor_setStorage4d", &THByteTensor_setStorage4d);
  ctx.bind("THByteTensor_narrow", &THByteTensor_narrow);
  ctx.bind("THByteTensor_select", &THByteTensor_select);
  ctx.bind("THByteTensor_transpose", &THByteTensor_transpose);
  ctx.bind("THByteTensor_unfold", &THByteTensor_unfold);
  ctx.bind("THByteTensor_squeeze", &THByteTensor_squeeze);
  ctx.bind("THByteTensor_squeeze1d", &THByteTensor_squeeze1d);
  ctx.bind("THByteTensor_unsqueeze1d", &THByteTensor_unsqueeze1d);
  ctx.bind("THByteTensor_isContiguous", &THByteTensor_isContiguous);
  ctx.bind("THByteTensor_isSameSizeAs", &THByteTensor_isSameSizeAs);
  ctx.bind("THByteTensor_isSetTo", &THByteTensor_isSetTo);
  ctx.bind("THByteTensor_isSize", &THByteTensor_isSize);
  ctx.bind("THByteTensor_nElement", &THByteTensor_nElement);
  ctx.bind("THByteTensor_retain", &THByteTensor_retain);
  ctx.bind("THByteTensor_free", &THByteTensor_free);
  ctx.bind("THByteTensor_freeCopyTo", &THByteTensor_freeCopyTo);
  // Byte Tensor slow access
  ctx.bind("THByteTensor_set1d", &THByteTensor_set1d);
  ctx.bind("THByteTensor_set2d", &THByteTensor_set2d);
  ctx.bind("THByteTensor_set3d", &THByteTensor_set3d);
  ctx.bind("THByteTensor_set4d", &THByteTensor_set4d);
  ctx.bind("THByteTensor_get1d", &THByteTensor_get1d);
  ctx.bind("THByteTensor_get2d", &THByteTensor_get2d);
  ctx.bind("THByteTensor_get3d", &THByteTensor_get3d);
  ctx.bind("THByteTensor_get4d", &THByteTensor_get4d);
  // Byte Tensor Math
  ctx.bind("THByteTensor_fill", &THByteTensor_fill);
  ctx.bind("THByteTensor_zero", &THByteTensor_zero);
  ctx.bind("THByteTensor_maskedFill", &THByteTensor_maskedFill);
  ctx.bind("THByteTensor_maskedCopy", &THByteTensor_maskedCopy);
  ctx.bind("THByteTensor_maskedSelect", &THByteTensor_maskedSelect);
  ctx.bind("THByteTensor_nonzero", &THByteTensor_nonzero);
  ctx.bind("THByteTensor_indexSelect", &THByteTensor_indexSelect);
  ctx.bind("THByteTensor_indexCopy", &THByteTensor_indexCopy);
  ctx.bind("THByteTensor_indexAdd", &THByteTensor_indexAdd);
  ctx.bind("THByteTensor_indexFill", &THByteTensor_indexFill);
  ctx.bind("THByteTensor_gather", &THByteTensor_gather);
  ctx.bind("THByteTensor_scatter", &THByteTensor_scatter);
  ctx.bind("THByteTensor_scatterAdd", &THByteTensor_scatterAdd);
  ctx.bind("THByteTensor_scatterFill", &THByteTensor_scatterFill);
  ctx.bind("THByteTensor_dot", &THByteTensor_dot);
  ctx.bind("THByteTensor_minall", &THByteTensor_minall);
  ctx.bind("THByteTensor_maxall", &THByteTensor_maxall);
  ctx.bind("THByteTensor_medianall", &THByteTensor_medianall);
  ctx.bind("THByteTensor_sumall", &THByteTensor_sumall);
  ctx.bind("THByteTensor_prodall", &THByteTensor_prodall);
  ctx.bind("THByteTensor_add", &THByteTensor_add);
  ctx.bind("THByteTensor_sub", &THByteTensor_sub);
  ctx.bind("THByteTensor_mul", &THByteTensor_mul);
  ctx.bind("THByteTensor_div", &THByteTensor_div);
  ctx.bind("THByteTensor_lshift", &THByteTensor_lshift);
  ctx.bind("THByteTensor_rshift", &THByteTensor_rshift);
  ctx.bind("THByteTensor_fmod", &THByteTensor_fmod);
  ctx.bind("THByteTensor_remainder", &THByteTensor_remainder);
  ctx.bind("THByteTensor_clamp", &THByteTensor_clamp);
  ctx.bind("THByteTensor_bitand", &THByteTensor_bitand);
  ctx.bind("THByteTensor_bitor", &THByteTensor_bitor);
  ctx.bind("THByteTensor_bitxor", &THByteTensor_bitxor);
  ctx.bind("THByteTensor_cadd", &THByteTensor_cadd);
  ctx.bind("THByteTensor_csub", &THByteTensor_csub);
  ctx.bind("THByteTensor_cmul", &THByteTensor_cmul);
  ctx.bind("THByteTensor_cpow", &THByteTensor_cpow);
  ctx.bind("THByteTensor_cdiv", &THByteTensor_cdiv);
  ctx.bind("THByteTensor_clshift", &THByteTensor_clshift);
  ctx.bind("THByteTensor_crshift", &THByteTensor_crshift);
  ctx.bind("THByteTensor_cfmod", &THByteTensor_cfmod);
  ctx.bind("THByteTensor_cremainder", &THByteTensor_cremainder);
  ctx.bind("THByteTensor_cbitand", &THByteTensor_cbitand);
  ctx.bind("THByteTensor_cbitor", &THByteTensor_cbitor);
  ctx.bind("THByteTensor_cbitxor", &THByteTensor_cbitxor);
  ctx.bind("THByteTensor_addcmul", &THByteTensor_addcmul);
  ctx.bind("THByteTensor_addcdiv", &THByteTensor_addcdiv);
  ctx.bind("THByteTensor_addmv", &THByteTensor_addmv);
  ctx.bind("THByteTensor_addmm", &THByteTensor_addmm);
  ctx.bind("THByteTensor_addr", &THByteTensor_addr);
  ctx.bind("THByteTensor_addbmm", &THByteTensor_addbmm);
  ctx.bind("THByteTensor_baddbmm", &THByteTensor_baddbmm);
  ctx.bind("THByteTensor_match", &THByteTensor_match);
  ctx.bind("THByteTensor_numel", &THByteTensor_numel);
  ctx.bind("THByteTensor_max", &THByteTensor_max);
  ctx.bind("THByteTensor_min", &THByteTensor_min);
  ctx.bind("THByteTensor_kthvalue", &THByteTensor_kthvalue);
  ctx.bind("THByteTensor_mode", &THByteTensor_mode);
  ctx.bind("THByteTensor_median", &THByteTensor_median);
  ctx.bind("THByteTensor_sum", &THByteTensor_sum);
  ctx.bind("THByteTensor_prod", &THByteTensor_prod);
  ctx.bind("THByteTensor_cumsum", &THByteTensor_cumsum);
  ctx.bind("THByteTensor_cumprod", &THByteTensor_cumprod);
  ctx.bind("THByteTensor_sign", &THByteTensor_sign);
  ctx.bind("THByteTensor_trace", &THByteTensor_trace);
  ctx.bind("THByteTensor_cross", &THByteTensor_cross);
  ctx.bind("THByteTensor_cmax", &THByteTensor_cmax);
  ctx.bind("THByteTensor_cmin", &THByteTensor_cmin);
  ctx.bind("THByteTensor_cmaxValue", &THByteTensor_cmaxValue);
  ctx.bind("THByteTensor_cminValue", &THByteTensor_cminValue);
  ctx.bind("THByteTensor_zeros", &THByteTensor_zeros);
  ctx.bind("THByteTensor_zerosLike", &THByteTensor_zerosLike);
  ctx.bind("THByteTensor_ones", &THByteTensor_ones);
  ctx.bind("THByteTensor_onesLike", &THByteTensor_onesLike);
  ctx.bind("THByteTensor_diag", &THByteTensor_diag);
  ctx.bind("THByteTensor_eye", &THByteTensor_eye);
  ctx.bind("THByteTensor_arange", &THByteTensor_arange);
  ctx.bind("THByteTensor_range", &THByteTensor_range);
  ctx.bind("THByteTensor_randperm", &THByteTensor_randperm);
  ctx.bind("THByteTensor_reshape", &THByteTensor_reshape);
  ctx.bind("THByteTensor_sort", &THByteTensor_sort);
  ctx.bind("THByteTensor_topk", &THByteTensor_topk);
  ctx.bind("THByteTensor_tril", &THByteTensor_tril);
  ctx.bind("THByteTensor_triu", &THByteTensor_triu);
  ctx.bind("THByteTensor_cat", &THByteTensor_cat);
  ctx.bind("THByteTensor_catArray", &THByteTensor_catArray);
  ctx.bind("THByteTensor_equal", &THByteTensor_equal);
  ctx.bind("THByteTensor_ltValue", &THByteTensor_ltValue);
  ctx.bind("THByteTensor_leValue", &THByteTensor_leValue);
  ctx.bind("THByteTensor_gtValue", &THByteTensor_gtValue);
  ctx.bind("THByteTensor_geValue", &THByteTensor_geValue);
  ctx.bind("THByteTensor_neValue", &THByteTensor_neValue);
  ctx.bind("THByteTensor_eqValue", &THByteTensor_eqValue);
  ctx.bind("THByteTensor_ltValueT", &THByteTensor_ltValueT);
  ctx.bind("THByteTensor_leValueT", &THByteTensor_leValueT);
  ctx.bind("THByteTensor_gtValueT", &THByteTensor_gtValueT);
  ctx.bind("THByteTensor_geValueT", &THByteTensor_geValueT);
  ctx.bind("THByteTensor_neValueT", &THByteTensor_neValueT);
  ctx.bind("THByteTensor_eqValueT", &THByteTensor_eqValueT);
  ctx.bind("THByteTensor_ltTensor", &THByteTensor_ltTensor);
  ctx.bind("THByteTensor_leTensor", &THByteTensor_leTensor);
  ctx.bind("THByteTensor_gtTensor", &THByteTensor_gtTensor);
  ctx.bind("THByteTensor_geTensor", &THByteTensor_geTensor);
  ctx.bind("THByteTensor_neTensor", &THByteTensor_neTensor);
  ctx.bind("THByteTensor_eqTensor", &THByteTensor_eqTensor);
  ctx.bind("THByteTensor_ltTensorT", &THByteTensor_ltTensorT);
  ctx.bind("THByteTensor_leTensorT", &THByteTensor_leTensorT);
  ctx.bind("THByteTensor_gtTensorT", &THByteTensor_gtTensorT);
  ctx.bind("THByteTensor_geTensorT", &THByteTensor_geTensorT);
  ctx.bind("THByteTensor_neTensorT", &THByteTensor_neTensorT);
  ctx.bind("THByteTensor_eqTensorT", &THByteTensor_eqTensorT);
  // Byte Random
  ctx.bind("THByteTensor_random", &THByteTensor_random);
  ctx.bind("THByteTensor_clampedRandom", &THByteTensor_clampedRandom);
  ctx.bind("THByteTensor_cappedRandom", &THByteTensor_cappedRandom);
  ctx.bind("THByteTensor_geometric", &THByteTensor_geometric);
  ctx.bind("THByteTensor_bernoulli", &THByteTensor_bernoulli);
  // Convolution
  ctx.bind("THByteTensor_validXCorr2Dptr", &THByteTensor_validXCorr2Dptr);
  ctx.bind("THByteTensor_validConv2Dptr", &THByteTensor_validConv2Dptr);
  ctx.bind("THByteTensor_fullXCorr2Dptr", &THByteTensor_fullXCorr2Dptr);
  ctx.bind("THByteTensor_fullConv2Dptr", &THByteTensor_fullConv2Dptr);
  ctx.bind("THByteTensor_validXCorr2DRevptr", &THByteTensor_validXCorr2DRevptr);
  ctx.bind("THByteTensor_conv2DRevger", &THByteTensor_conv2DRevger);
  ctx.bind("THByteTensor_conv2DRevgerm", &THByteTensor_conv2DRevgerm);
  ctx.bind("THByteTensor_conv2Dger", &THByteTensor_conv2Dger);
  ctx.bind("THByteTensor_conv2Dmv", &THByteTensor_conv2Dmv);
  ctx.bind("THByteTensor_conv2Dmm", &THByteTensor_conv2Dmm);
  ctx.bind("THByteTensor_conv2Dmul", &THByteTensor_conv2Dmul);
  ctx.bind("THByteTensor_conv2Dcmul", &THByteTensor_conv2Dcmul);
  ctx.bind("THByteTensor_validXCorr3Dptr", &THByteTensor_validXCorr3Dptr);
  ctx.bind("THByteTensor_validConv3Dptr", &THByteTensor_validConv3Dptr);
  ctx.bind("THByteTensor_fullXCorr3Dptr", &THByteTensor_fullXCorr3Dptr);
  ctx.bind("THByteTensor_fullConv3Dptr", &THByteTensor_fullConv3Dptr);
  ctx.bind("THByteTensor_validXCorr3DRevptr", &THByteTensor_validXCorr3DRevptr);
  ctx.bind("THByteTensor_conv3DRevger", &THByteTensor_conv3DRevger);
  ctx.bind("THByteTensor_conv3Dger", &THByteTensor_conv3Dger);
  ctx.bind("THByteTensor_conv3Dmv", &THByteTensor_conv3Dmv);
  ctx.bind("THByteTensor_conv3Dmul", &THByteTensor_conv3Dmul);
  ctx.bind("THByteTensor_conv3Dcmul", &THByteTensor_conv3Dcmul);
  
}
}
