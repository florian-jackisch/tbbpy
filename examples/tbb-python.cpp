#include <cmath>

#include <mutex>
#include <string>
#include <vector>

#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include <tbb/flow_graph.h>

static std::mutex pythonGil;

class PythonInterpreter {
 public:
  // Create a new subinterpreter
  PythonInterpreter() : interpreter{newInterpreter()} {}

  // Not copyable
  PythonInterpreter(const PythonInterpreter &) = delete;
  PythonInterpreter &operator=(const PythonInterpreter &) = delete;

  // Moveable
  PythonInterpreter(PythonInterpreter &&other) noexcept
      : interpreter{other.interpreter} {
    other.interpreter = nullptr;
  }
  PythonInterpreter &operator=(PythonInterpreter &&other) noexcept {
    interpreter = other.interpreter;
    other.interpreter = nullptr;
    return *this;
  }

  // Delete the sub-interpreter
  ~PythonInterpreter() {
    if (interpreter != nullptr) {
      std::lock_guard<std::mutex> lock{pythonGil};
      PythonInterpreterGuard interpreterLock{interpreter};
      Py_EndInterpreter(interpreter);
    }
  }

  template <typename F, typename... Args>
  auto operator()(F &&f, Args... args) {
    std::lock_guard<std::mutex> lock{pythonGil};
    PythonInterpreterGuard interpreterLock{interpreter};
    return std::forward<F>(f)(args...);
  }

 private:
  PyThreadState *interpreter;

  struct PythonInterpreterGuard {
    PythonInterpreterGuard(PyThreadState *i) { PyThreadState_Swap(i); }
    ~PythonInterpreterGuard() { PyThreadState_Swap(nullptr); }
  };

  PyThreadState *newInterpreter() {
    std::lock_guard<std::mutex> lock{pythonGil};
    return Py_NewInterpreter();
  }
};

class PythonMainInterpreter {
 public:
  PythonMainInterpreter() {
    std::lock_guard<std::mutex> lock{pythonGil};
    Py_Initialize();
    PyEval_InitThreads();
    interp_main = PyThreadState_Get();
  }

  ~PythonMainInterpreter() {
    std::lock_guard<std::mutex> lock{pythonGil};
    PyThreadState_Swap(interp_main);
    Py_Finalize();
  }

 private:
  PyThreadState *interp_main;
};

static PythonMainInterpreter pythonInterpreter;

template <typename T>
std::vector<T> copyToVector(const pybind11::array_t<T> &a) {
  return std::vector<T>(a.data(), a.data() + a.size());
}

template <typename T>
pybind11::array copyToNumpyArray(const std::vector<T> &v) {
  return pybind11::array(v.size(), v.data());
}

class Linspace {
 public:
  Linspace(size_t maxCount)
      : maxCount{maxCount}, np(pybind11::module::import("numpy")) {}

  bool operator()(std::vector<double> &out) {
    if (count >= maxCount) {
      return false;
    }
    auto nparray = interpreter([this]() {
      return np.attr("linspace")(0, 2 * M_PI).cast<pybind11::array_t<double>>();
    });
    out = copyToVector(nparray);
    ++count;
    return true;
  }

 private:
  size_t count = 0;
  const size_t maxCount;
  PythonInterpreter interpreter;
  pybind11::module np;
};

class Cos {
 public:
  Cos() : np{pybind11::module::import("numpy")} {}

  std::vector<double> operator()(const std::vector<double> &v) {
    return interpreter([this, &v]() { return cos(v); });
  }

 private:
  PythonInterpreter interpreter;
  pybind11::module np;

  std::vector<double> cos(const std::vector<double> &v) {
    auto b =
        np.attr("cos")(copyToNumpyArray(v)).cast<pybind11::array_t<double>>();
    return copyToVector(b);
  }
};

int main() {
  using namespace tbb::flow;

  {
    auto p = PythonInterpreter{};
    p([]() {
      pybind11::print("Using python from within a multithreaded C++ program.");
    });
  }

  graph g;
  // The source produes
  Linspace linspace(1000);
  source_node<std::vector<double>> sourceNode{
      g, [&linspace](auto &out) { return linspace(out); }};

  Cos cos;
  function_node<std::vector<double>, std::vector<double>> cosNode{
      g, 1, [&cos](const auto &v) { return cos(v); }};

  unsigned count = 0;
  function_node<std::vector<double>> countNode{
      g, 1, [&count](const auto &) { ++count; }};
  make_edge(sourceNode, cosNode);
  make_edge(cosNode, countNode);
  g.wait_for_all();

  {
    auto p = PythonInterpreter{};
    p([count]() {
      pybind11::exec("print('Worked with " + std::to_string(count) +
                     " vectors in python and C++!')");
    });
  }
}
