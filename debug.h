#define BENCHMARK(label) \
  auto label##_start = std::chrono::high_resolution_clock::now();

#define BENCHMARKEND(label) \
  auto label##_end = chrono::high_resolution_clock::now(); \
  cout << #label << " elapsed: " << chrono::duration_cast<chrono::milliseconds>(label##_end - label##_start).count() << " ms\n";
