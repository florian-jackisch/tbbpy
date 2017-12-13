#include <iostream>

#include <tbb/flow_graph.h>

int main() {
  tbb::flow::graph g;
  tbb::flow::function_node<int> f{g, 1, [](auto i) { std::cout << i << '\n'; }};

  f.try_put(42);
  g.wait_for_all();
}