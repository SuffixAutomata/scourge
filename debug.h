#define BENCHMARK(label)                                                       \
  auto label##_start = std::chrono::high_resolution_clock::now();

#define BENCHMARKEND(label)                                                    \
  auto label##_end = std::chrono::high_resolution_clock::now();                \
  DEBUG << #label << " elapsed: "                                             \
        << std::chrono::duration_cast<std::chrono::milliseconds>(label##_end -  \
                                                                 label##_start) \
               .count()                                                         \
        << " ms\n";
