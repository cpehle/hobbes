#include <iostream>
#include <stdexcept>
#include <hobbes/hobbes.H>

int applyDiscreteWeighting(int x, int y) {
  return x * y;
}

typedef std::pair<size_t, const hobbes::array<char>*> MyRecord;

const hobbes::array<MyRecord>* loadRecords(int key) {
  static const char* names[] = { "chicken", "hot dog", "foobar", "jimmy" };

  auto rs = hobbes::makeArray<MyRecord>(key);
  for (size_t i = 0; i < rs->size; ++i) {
    rs->data[i].first  = i;
    rs->data[i].second = hobbes::makeString(names[i % 4]);
  }

  return rs;
}

DEFINE_STRUCT(MyStructuredRecord,
  (size_t, index),
  (const hobbes::array<char>*, name)
);

const hobbes::array<MyStructuredRecord>* loadStructRecords(int key) {
  static const char* names[] = { "chicken", "hot dog", "foobar", "jimmy" };

  auto rs = hobbes::makeArray<MyStructuredRecord>(key);
  for (size_t i = 0; i < rs->size; ++i) {
    rs->data[i].index  = i;
    rs->data[i].name = hobbes::makeString(names[i % 4]);
  }

  return rs;
}

int main() {
  hobbes::cc c;

  c.bind("applyDiscreteWeighting", &applyDiscreteWeighting);
  c.bind("loadRecords", &loadRecords);
  c.bind("loadStructRecords", &loadStructRecords);
  
  while (std::cin) {
    std::cout << "> " << std::flush;

    std::string line;
    std::getline(std::cin, line);
    if (line == ":q") break;

    try {
      c.compileFn<void()>("print(" + line + ")")();
    } catch (std::exception& ex) {
      std::cout << "*** " << ex.what();
    }

    std::cout << std::endl;
    hobbes::resetMemoryPool();
  }

  return 0;
}
