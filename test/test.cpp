#define CATCH_CONFIG_MAIN

#include <leftist_heap/heap.hpp>

#include <catch2/catch.hpp>

using MyHeap =
    Heap<int, std::less<>, shared_ptr_mem, Node<int, std::shared_ptr<void>>>;

TEST_CASE("A new Heap is empty") {
  MyHeap h{};
  REQUIRE(h.empty());
}

TEST_CASE("A heap with one element is not empty") {
  MyHeap h0{};
  auto   h1 = into(h0, std::initializer_list<int>{3});
  REQUIRE(!h1.empty());
}

TEST_CASE("Peeking a heap with one element gives you that element") {
  MyHeap h0{};
  auto   h1 = into(h0, std::initializer_list<int>{3});
  REQUIRE(h1.peek() == 3);
}

TEST_CASE("Popping a heap with one element gives you the empty heap") {
  MyHeap h0{};
  auto   h1 = h0.cons(3);
  REQUIRE(h1.pop().empty());
}

TEST_CASE("Popping a heap sorts") {
  MyHeap h0{};
  auto   h1 = into(h0, std::initializer_list<int>{5, 1, 2});
  REQUIRE(h1.pop().peek() == 2);
}

TEST_CASE("Vector heap can push"){
  using node = Node<int, size_t>;
  using VecHeap = Heap<int, std::less<>, vector_mem, node>;

  std::vector<node> block{};

  vector_mem<node> mem{&block};

  VecHeap h0{mem};
  auto h1 = h0.cons(3);
  REQUIRE(h1.peek()==3);
}