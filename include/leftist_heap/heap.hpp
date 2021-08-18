#ifndef LEFTIST_HEAP_HPP_INCLUDE_GUARD
#define LEFTIST_HEAP_HPP_INCLUDE_GUARD

#include "macros.hpp"
#include "asserts.hpp"

#include <memory>
#include <algorithm>
#include <ranges>

#include <limits>
#include <cstdint>

template<class To, class From>
To narrow(From const x) {
  To y = static_cast<To>(x);
  ASSERT(x == static_cast<From>(y));
  return y;
}

template<class T>
constexpr bool easy_to_copy =
    std::is_trivially_copyable_v<T> && sizeof(T) <= 3 * sizeof(void*);

template<class T>
using Read = std::conditional_t<easy_to_copy<T>, T const, T const&>;

static_assert(std::is_reference_v<Read<std::shared_ptr<void>>>);
static_assert(!std::is_reference_v<Read<int>>);

// TODO: noexcept correctness and assertions?
// we don't necessarily want to abort if an assertion is triggered
// we may want to throw and then autosave
// But if we won't check the assertions, we do want noexcept
// how important is that? i suspect most of these get inlined

template<class T, class vector = std::vector<T>, class Index_Num = size_t>
// It makes sense to use this with a boost::static_vector for example
// Or to use different allocators.
struct vector_mem {
  // TODO: probably needs better name. Pointdex? Inder?
  // key handle reference
  using Index = Index_Num;
  vector* block;

  // TODO: what is the right way to pass index?
  // ideally they are small, so by value
  // but they might do resource management, so by reference (shared_ptr
  // -- prevent extra copy) I want to use a Read template. Is this too
  // complicated?
  T const& operator[](Index i) const {
    ASSERT(!is_null(i));
    ASSERT_PARANOID(0 < i);
    WITH_DIAGNOSTIC_TWEAK(
        IGNORE_WASSUME, ASSERT_PARANOID(i <= block->size());)
    // why does clang think vector::size() has sideffects?
    return (*block)[i - 1];
  }

  Index null() const noexcept { return 0; }
  bool  is_null(Read<Index> i) const noexcept { return i == 0; }

  Index make_index(auto&&... args) {
    block->emplace_back(FWD(args)...);
    return block->size();
  }
};

template<class T>
struct shared_ptr_mem {
  using Index = std::shared_ptr<void>;

  T const& operator[](Index const& i) const {
    ASSERT(!is_null(i));
    return *static_cast<T*>(i.get());
  }

  Index null() const { return {}; }
  bool  is_null(Index const& x) const { return x == nullptr; }
  auto make_index(auto&&... args) ARROW(std::make_shared<T>(FWD(args)...))
};

template<class Less = std::less<>>
constexpr auto cmp_by(auto&& f, Less&& less = {}) noexcept {
  return [ f = FWD(f), less = FWD(less) ] FN(less(f(_0), f(_1)));
}

template<class T, class index, class Rank = std::uint8_t>
struct Node {
  using Index = index;
  // TODO: how much can these be compressed?
  // realistically, Index cannot be smaller than uint8: 2^4= 16 very small heap
  // when is it small enough to be worth just copying a vector/vector heap?
  T     elt;
  Index left;
  Index right;
  Rank  rank; // should we store rank-1 instead? Might increase range but complicates logic
  // rank <= floor(log(n+1))
  // this can probably be much smaller: ~ log(bit_width<Index>)
  // if index =uint64
  // max # is #uint64s - 1 (-1 for null) = uint64max = 2^64-1
  // so rank <= 64

  static Rank rank_of(auto const mem, Read<Index> n) {
    return mem.is_null(n) ? Rank{} : mem[n].rank;
  }

  static Index
      make(auto mem, T e, Read<Index> node1, Read<Index> node2) {
    auto const& [r, l] =
        std::minmax(node1, node2, cmp_by([&] FN(rank_of(mem, _))));
    auto const old_rank = rank_of(mem, r);
    return mem.template make_index(Node{
        .elt   = std::move(e),
        .left  = l,
        .right = r,
        .rank  = narrow<Rank>(old_rank + 1)});
  }

  static Index
      merge(auto mem, auto less, Read<Index> node1, Read<Index> node2) {
    if(mem.is_null(node1)) return node2;
    if(mem.is_null(node2)) return node1;
    else if(less(mem[node2].elt, mem[node1].elt))
      return make(mem,
                  mem[node2].elt,
                  mem[node2].left,
                  merge(mem, less, node1, mem[node2].right));
    else
      return make(mem,
                  mem[node1].elt,
                  mem[node1].left,
                  merge(mem, less, mem[node1].right, node2));
    // always merge with the right b/c of leftist property
  }

  template<class size_type>
  static size_type count(auto const mem, Read<Index> node) {
    return mem.is_null(node)
             ? size_type{}
             : (size_type{1} + count(mem[node].left)
                + count(mem[node].right));
  }
};

// as of now we need to say the mem and node types in order to construct
// the vector for the vector memory
template<class T, class Less, class Mem_, class Node>
// TODO: Are there other useful definitions of Node?
// if element has ~6 unused bits, we can put rank in it
class Heap {
  using node = Node;
  using Mem  = Mem_;
  using idx  = typename node::Index;

  [[no_unique_address]] Less        less_;
  [[no_unique_address]] mutable Mem mem_;
  idx                               root_{};

  Heap(Mem mem, Less less, idx h)
      : less_{std::move(less)}, mem_{std::move(mem)}, root_{std::move(h)} {}

 public:
  using size_type = std::size_t;
  explicit Heap(Mem mem = {}, Less less = {})
      : less_{std::move(less)}, mem_{std::move(mem)} {}

  bool empty() const { return mem_.is_null(root_); }

  T const& peek() const {
    ASSERT(!empty());
    return mem_[root_].elt;
  }

  Heap pop() const {
    ASSERT(!empty());
    return {mem_,
            less_,
            node::merge(mem_, less_, mem_[root_].left, mem_[root_].right)};
  }

  Heap cons(T e) const {
    return {mem_,
            less_,
            node::merge(mem_,
                        less_,
                        root_,
                        node::make(mem_, e, mem_.null(), mem_.null()))};
  }

  size_type size() const {
    return node::template count<size_type>(root_);
  }
};

template<class Coll, std::ranges::range Rng>
inline auto into(Coll coll, Rng const data) {
  for(auto x : data) coll = coll.cons(x);
  return coll;
}
#endif // LEFTIST_HEAP_HPP_INCLUDE_GUARD
