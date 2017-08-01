#include "hobbes/net.H"

#include <iostream>

// our RPC signature
DEFINE_NET_CLIENT(
  Connection,                       // a name for this connection profile
  (add, int(int,int), "\\x y.x+y"), // a function "add" of type int(int,int) remotely implemented by the expression '\x y.x+y'
  (mul, int(int,int), "\\x y.x*y")  // as above but "mul" for the expression '\x y.x*y'
);

int main(int, char**) {
  try {
    Connection c("localhost:8080");

    std::cout << "c.add(1,2) = " << c.add(1,2) << std::endl
              << "c.mul(1,2) = " << c.mul(1,2) << std::endl;

  } catch (std::exception& e) {
    std::cout << "*** " << e.what() << std::endl;
  }
  return 0;
}
