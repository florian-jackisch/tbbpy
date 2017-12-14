#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include <tbb/flow_graph.h>

namespace tbbpy {

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

}  // namespace tbbpy

class Linspace {
 public:
  Linspace(size_t maxCount)
      : maxCount{maxCount}, np(pybind11::module::import("numpy")) {}

  bool operator()(std::vector<double> &out) {
    if (count >= maxCount) {
      return false;
    }
    auto nparray = interpreter([this]() {
      return np.attr("linspace")(0, 10).cast<pybind11::array_t<double>>();
    });
    out = tbbpy::copyToVector(nparray);
    ++count;
    return true;
  }

 private:
  size_t count = 0;
  const size_t maxCount;
  tbbpy::PythonInterpreter interpreter;
  pybind11::module np;
};

class Print {
 public:
  Print() : np{pybind11::module::import("numpy")} {}

  void operator()(const std::vector<double> &v) {
    interpreter([this, &v]() { print(v); });
  }

 private:
  tbbpy::PythonInterpreter interpreter;
  pybind11::module np;

  void print(const std::vector<double> &v) {
    auto a = np.attr("zeros")(v.size()).cast<pybind11::array_t<double>>();
    std::copy(v.begin(), v.end(), a.mutable_data());
    pybind11::print(a);
  }
};

class Cos {
 public:
  Cos() : np{pybind11::module::import("numpy")} {}

  std::vector<double> operator()(const std::vector<double> &v) {
    return interpreter([this, &v]() { return cos(v); });
  }

 private:
  tbbpy::PythonInterpreter interpreter;
  pybind11::module np;

  std::vector<double> cos(const std::vector<double> &v) {
    auto b = np.attr("cos")(tbbpy::copyToNumpyArray(v))
                 .cast<pybind11::array_t<double>>();
    return tbbpy::copyToVector(b);
  }
};

int main() {
  using namespace tbbpy;
  using namespace tbb::flow;

  graph g;
  Linspace linspace(10);
  source_node<std::vector<double>> sourceNode{
      g, [&linspace](auto &out) { return linspace(out); }};
  Cos cos;
  function_node<std::vector<double>, std::vector<double>> cosNode{
      g, 1, [&cos](const auto &v) { return cos(v); }};
  Print print;
  function_node<std::vector<double>> printNode{
      g, 1, [&print](const auto &v) { print(v); }};
  make_edge(sourceNode, cosNode);
  make_edge(cosNode, printNode);
  g.wait_for_all();
}
