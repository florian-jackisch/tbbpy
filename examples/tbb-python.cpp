#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include <tbb/flow_graph.h>

std::mutex pythonGil;

template <typename T>
std::vector<T> copyToVector(const pybind11::array_t<T>& a) {
  return std::vector<T>(a.data(), a.data() + a.size());
}

class PythonInterpreter {
 public:
  // Create a new subinterpreter
  PythonInterpreter() : interpreter{newInterpreter()} {}

  // Not copyable
  PythonInterpreter(const PythonInterpreter&) = delete;
  PythonInterpreter& operator=(const PythonInterpreter&) = delete;

  // Moveable
  PythonInterpreter(PythonInterpreter&& other) noexcept
      : interpreter{other.interpreter} {}
  PythonInterpreter& operator=(PythonInterpreter&& other) noexcept {
    interpreter = other.interpreter;
    return *this;
  }

  // Delete the sub-interpreter
  ~PythonInterpreter() {
    std::lock_guard<std::mutex> lock{pythonGil};
    PythonInterpreterGuard interpreterLock{interpreter};
    Py_EndInterpreter(interpreter);
  }

  template <typename F, typename... Args>
  auto operator()(F&& f, Args... args) {
    std::lock_guard<std::mutex> lock{pythonGil};
    PythonInterpreterGuard interpreterLock{interpreter};
    return std::forward<F>(f)(args...);
  }

 private:
  PyThreadState* interpreter;

  struct PythonInterpreterGuard {
    PythonInterpreterGuard(PyThreadState* i) { PyThreadState_Swap(i); }
    ~PythonInterpreterGuard() { PyThreadState_Swap(nullptr); }
  };

  PyThreadState* newInterpreter() {
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
  PyThreadState* interp_main;
};

class Linspace {
 public:
  Linspace() : np(pybind11::module::import("numpy")) {}
  void operator()() {
    auto f = [this]() { pybind11::print(np.attr("linspace")(0, 10)); };
    interpreter(f);
  }

 private:
  PythonInterpreter interpreter;
  pybind11::module np;
};

int main() {
  PythonMainInterpreter p;

  Linspace f1;
  auto fut1 = std::async(std::launch::async, [&]() { f1(); });

  PythonInterpreter interp2;
  auto f2 = []() {
    pybind11::print("Different thread");
    pybind11::exec(R"(
          import os
          print(dir())
      )");
  };
  auto fut2 = std::async(std::launch::async, [&]() { interp2(f2); });

  fut1.wait();
  fut2.wait();

  return 0;
}