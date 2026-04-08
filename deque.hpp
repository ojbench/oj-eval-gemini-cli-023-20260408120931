#ifndef SJTU_DEQUE_HPP
#define SJTU_DEQUE_HPP

#include "exceptions.hpp"
#include <cstddef>
#include <utility>
#include <type_traits>
#include <cstring>

namespace sjtu {

template <class T> class deque {
private:
    static const size_t BLOCK_CAP = 512;
    struct Block {
        Block* prev;
        Block* next;
        size_t size;
        T* data;
        
        Block() : prev(nullptr), next(nullptr), size(0) {
            data = reinterpret_cast<T*>(operator new(BLOCK_CAP * sizeof(T)));
        }
        ~Block() {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_t i = 0; i < size; ++i) {
                    data[i].~T();
                }
            }
            operator delete(data);
        }
        void split() {
            Block* new_block = new Block();
            size_t half = size / 2;
            if constexpr (std::is_trivially_copy_constructible_v<T> && std::is_trivially_destructible_v<T>) {
                std::memcpy(new_block->data, data + half, (size - half) * sizeof(T));
            } else {
                for (size_t i = half; i < size; ++i) {
                    new (new_block->data + (i - half)) T(std::move(data[i]));
                    data[i].~T();
                }
            }
            new_block->size = size - half;
            size = half;
            
            new_block->next = next;
            new_block->prev = this;
            if (next) next->prev = new_block;
            next = new_block;
        }
        void merge_next() {
            Block* next_block = next;
            if constexpr (std::is_trivially_copy_constructible_v<T> && std::is_trivially_destructible_v<T>) {
                std::memcpy(data + size, next_block->data, next_block->size * sizeof(T));
            } else {
                for (size_t i = 0; i < next_block->size; ++i) {
                    new (data + size + i) T(std::move(next_block->data[i]));
                    next_block->data[i].~T();
                }
            }
            size += next_block->size;
            next_block->size = 0;
            
            next = next_block->next;
            if (next) next->prev = this;
            delete next_block;
        }
        void insert(size_t pos, const T& value) {
            if constexpr (std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T> && std::is_trivially_destructible_v<T>) {
                if (pos < size) {
                    std::memmove(data + pos + 1, data + pos, (size - pos) * sizeof(T));
                }
                new (data + pos) T(value);
            } else {
                if (pos < size) {
                    new (data + size) T(std::move(data[size - 1]));
                    for (size_t i = size - 1; i > pos; --i) {
                        data[i].~T();
                        new (data + i) T(std::move(data[i - 1]));
                    }
                    data[pos].~T();
                    new (data + pos) T(value);
                } else {
                    new (data + pos) T(value);
                }
            }
            ++size;
        }
        void erase(size_t pos) {
            if constexpr (std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T> && std::is_trivially_destructible_v<T>) {
                if (pos < size - 1) {
                    std::memmove(data + pos, data + pos + 1, (size - pos - 1) * sizeof(T));
                }
            } else {
                for (size_t i = pos; i < size - 1; ++i) {
                    data[i].~T();
                    new (data + i) T(std::move(data[i + 1]));
                }
                data[size - 1].~T();
            }
            --size;
        }
    };
    
    Block* head;
    Block* tail;
    size_t total_size;
    
    void check_split(Block* b) {
        if (b->size == BLOCK_CAP) {
            b->split();
            if (b == tail) {
                tail = b->next;
            }
        }
    }
    
    void check_merge(Block* b) {
        if (b->next && b->size + b->next->size <= BLOCK_CAP / 2) {
            if (b->next == tail) {
                tail = b;
            }
            b->merge_next();
        }
    }
    
    void check_empty(Block* b) {
        if (b->size == 0 && head != tail) {
            if (b == head) {
                head = b->next;
                head->prev = nullptr;
            } else if (b == tail) {
                tail = b->prev;
                tail->next = nullptr;
            } else {
                b->prev->next = b->next;
                b->next->prev = b->prev;
            }
            delete b;
        }
    }

    mutable Block* last_accessed_block;
    mutable size_t last_accessed_idx;

    void invalidate_cache() const {
        last_accessed_block = nullptr;
        last_accessed_idx = 0;
    }

public:
  class const_iterator;
  class iterator {
  public:
    deque* q;
    Block* b;
    size_t pos;
    
    iterator(deque* q = nullptr, Block* b = nullptr, size_t pos = 0) : q(q), b(b), pos(pos) {}

    iterator operator+(const int &n) const {
        if (n < 0) return operator-(-n);
        if (n == 0) return *this;
        if (b == nullptr) throw invalid_iterator();
        
        Block* curr_b = b;
        size_t curr_pos = pos + n;
        while (curr_b && curr_pos >= curr_b->size) {
            curr_pos -= curr_b->size;
            curr_b = curr_b->next;
        }
        if (curr_b == nullptr) {
            if (curr_pos == 0) return iterator(q, nullptr, 0);
            throw index_out_of_bound();
        }
        return iterator(q, curr_b, curr_pos);
    }
    
    iterator operator-(const int &n) const {
        if (n < 0) return operator+(-n);
        if (n == 0) return *this;
        
        Block* curr_b = b;
        size_t curr_pos = pos;
        
        if (curr_b == nullptr) {
            curr_b = q->tail;
            if (curr_b == nullptr || curr_b->size == 0) throw invalid_iterator();
            curr_pos = curr_b->size;
        }
        
        int rem = n;
        while (curr_b && rem > curr_pos) {
            rem -= (curr_pos + 1);
            curr_b = curr_b->prev;
            if (curr_b) curr_pos = curr_b->size - 1;
        }
        if (curr_b == nullptr) throw index_out_of_bound();
        return iterator(q, curr_b, curr_pos - rem);
    }

    int operator-(const iterator &rhs) const {
        if (q != rhs.q) throw invalid_iterator();
        return q->get_index(*this) - q->get_index(rhs);
    }
    
    iterator &operator+=(const int &n) {
        *this = *this + n;
        return *this;
    }
    iterator &operator-=(const int &n) {
        *this = *this - n;
        return *this;
    }

    iterator operator++(int) {
        iterator tmp = *this;
        ++(*this);
        return tmp;
    }
    iterator &operator++() {
        if (b == nullptr) throw invalid_iterator();
        if (pos + 1 < b->size) {
            ++pos;
        } else {
            b = b->next;
            pos = 0;
        }
        return *this;
    }
    iterator operator--(int) {
        iterator tmp = *this;
        --(*this);
        return tmp;
    }
    iterator &operator--() {
        if (b == nullptr) {
            b = q->tail;
            if (b == nullptr) throw invalid_iterator();
            pos = b->size - 1;
        } else {
            if (pos > 0) {
                --pos;
            } else {
                b = b->prev;
                if (b == nullptr) throw invalid_iterator();
                pos = b->size - 1;
            }
        }
        return *this;
    }

    T &operator*() const {
        if (b == nullptr) throw invalid_iterator();
        return b->data[pos];
    }
    T *operator->() const noexcept {
        if (b == nullptr) return nullptr;
        return &(b->data[pos]);
    }

    bool operator==(const iterator &rhs) const {
        return q == rhs.q && b == rhs.b && pos == rhs.pos;
    }
    bool operator==(const const_iterator &rhs) const {
        return q == rhs.q && b == rhs.b && pos == rhs.pos;
    }
    bool operator!=(const iterator &rhs) const {
        return !(*this == rhs);
    }
    bool operator!=(const const_iterator &rhs) const {
        return !(*this == rhs);
    }
  };

  class const_iterator {
  public:
    const deque* q;
    const Block* b;
    size_t pos;
    
    const_iterator(const deque* q = nullptr, const Block* b = nullptr, size_t pos = 0) : q(q), b(b), pos(pos) {}
    const_iterator(const iterator& other) : q(other.q), b(other.b), pos(other.pos) {}

    const_iterator operator+(const int &n) const {
        if (n < 0) return operator-(-n);
        if (n == 0) return *this;
        if (b == nullptr) throw invalid_iterator();
        
        const Block* curr_b = b;
        size_t curr_pos = pos + n;
        while (curr_b && curr_pos >= curr_b->size) {
            curr_pos -= curr_b->size;
            curr_b = curr_b->next;
        }
        if (curr_b == nullptr) {
            if (curr_pos == 0) return const_iterator(q, nullptr, 0);
            throw index_out_of_bound();
        }
        return const_iterator(q, curr_b, curr_pos);
    }
    
    const_iterator operator-(const int &n) const {
        if (n < 0) return operator+(-n);
        if (n == 0) return *this;
        
        const Block* curr_b = b;
        size_t curr_pos = pos;
        
        if (curr_b == nullptr) {
            curr_b = q->tail;
            if (curr_b == nullptr || curr_b->size == 0) throw invalid_iterator();
            curr_pos = curr_b->size;
        }
        
        int rem = n;
        while (curr_b && rem > curr_pos) {
            rem -= (curr_pos + 1);
            curr_b = curr_b->prev;
            if (curr_b) curr_pos = curr_b->size - 1;
        }
        if (curr_b == nullptr) throw index_out_of_bound();
        return const_iterator(q, curr_b, curr_pos - rem);
    }

    int operator-(const const_iterator &rhs) const {
        if (q != rhs.q) throw invalid_iterator();
        return q->get_index(*this) - q->get_index(rhs);
    }
    
    const_iterator &operator+=(const int &n) {
        *this = *this + n;
        return *this;
    }
    const_iterator &operator-=(const int &n) {
        *this = *this - n;
        return *this;
    }

    const_iterator operator++(int) {
        const_iterator tmp = *this;
        ++(*this);
        return tmp;
    }
    const_iterator &operator++() {
        if (b == nullptr) throw invalid_iterator();
        if (pos + 1 < b->size) {
            ++pos;
        } else {
            b = b->next;
            pos = 0;
        }
        return *this;
    }
    const_iterator operator--(int) {
        const_iterator tmp = *this;
        --(*this);
        return tmp;
    }
    const_iterator &operator--() {
        if (b == nullptr) {
            b = q->tail;
            if (b == nullptr) throw invalid_iterator();
            pos = b->size - 1;
        } else {
            if (pos > 0) {
                --pos;
            } else {
                b = b->prev;
                if (b == nullptr) throw invalid_iterator();
                pos = b->size - 1;
            }
        }
        return *this;
    }

    const T &operator*() const {
        if (b == nullptr) throw invalid_iterator();
        return b->data[pos];
    }
    const T *operator->() const noexcept {
        if (b == nullptr) return nullptr;
        return &(b->data[pos]);
    }

    bool operator==(const iterator &rhs) const {
        return q == rhs.q && b == rhs.b && pos == rhs.pos;
    }
    bool operator==(const const_iterator &rhs) const {
        return q == rhs.q && b == rhs.b && pos == rhs.pos;
    }
    bool operator!=(const iterator &rhs) const {
        return !(*this == rhs);
    }
    bool operator!=(const const_iterator &rhs) const {
        return !(*this == rhs);
    }
  };

  int get_index(const iterator& it) const {
      if (it.b == nullptr) return total_size;
      int idx = 0;
      Block* curr = head;
      while (curr && curr != it.b) {
          idx += curr->size;
          curr = curr->next;
      }
      if (curr == nullptr) throw invalid_iterator();
      return idx + it.pos;
  }

  int get_index(const const_iterator& it) const {
      if (it.b == nullptr) return total_size;
      int idx = 0;
      const Block* curr = head;
      while (curr && curr != it.b) {
          idx += curr->size;
          curr = curr->next;
      }
      if (curr == nullptr) throw invalid_iterator();
      return idx + it.pos;
  }

  deque() : head(nullptr), tail(nullptr), total_size(0), last_accessed_block(nullptr), last_accessed_idx(0) {}
  
  deque(const deque &other) : head(nullptr), tail(nullptr), total_size(0), last_accessed_block(nullptr), last_accessed_idx(0) {
      Block* curr = other.head;
      while (curr) {
          for (size_t i = 0; i < curr->size; ++i) {
              push_back(curr->data[i]);
          }
          curr = curr->next;
      }
  }

  ~deque() {
      clear();
  }

  deque &operator=(const deque &other) {
      if (this == &other) return *this;
      clear();
      invalidate_cache();
      Block* curr = other.head;
      while (curr) {
          for (size_t i = 0; i < curr->size; ++i) {
              push_back(curr->data[i]);
          }
          curr = curr->next;
      }
      return *this;
  }

  T &at(const size_t &pos) {
      return operator[](pos);
  }
  const T &at(const size_t &pos) const {
      return operator[](pos);
  }
  T &operator[](const size_t &pos) {
      if (pos >= total_size) throw index_out_of_bound();
      Block* curr;
      size_t curr_idx;
      
      if (last_accessed_block) {
          if (pos >= last_accessed_idx) {
              curr = last_accessed_block;
              curr_idx = last_accessed_idx;
          } else {
              if (pos > last_accessed_idx / 2) {
                  curr = last_accessed_block;
                  curr_idx = last_accessed_idx;
                  while (curr && curr_idx > pos) {
                      curr = curr->prev;
                      if (curr) curr_idx -= curr->size;
                  }
                  last_accessed_block = curr;
                  last_accessed_idx = curr_idx;
                  return curr->data[pos - curr_idx];
              } else {
                  curr = head;
                  curr_idx = 0;
              }
          }
      } else {
          curr = head;
          curr_idx = 0;
      }
      
      while (curr && pos >= curr_idx + curr->size) {
          curr_idx += curr->size;
          curr = curr->next;
      }
      
      last_accessed_block = curr;
      last_accessed_idx = curr_idx;
      return curr->data[pos - curr_idx];
  }
  const T &operator[](const size_t &pos) const {
      if (pos >= total_size) throw index_out_of_bound();
      Block* curr;
      size_t curr_idx;
      
      if (last_accessed_block) {
          if (pos >= last_accessed_idx) {
              curr = last_accessed_block;
              curr_idx = last_accessed_idx;
          } else {
              if (pos > last_accessed_idx / 2) {
                  curr = last_accessed_block;
                  curr_idx = last_accessed_idx;
                  while (curr && curr_idx > pos) {
                      curr = curr->prev;
                      if (curr) curr_idx -= curr->size;
                  }
                  last_accessed_block = curr;
                  last_accessed_idx = curr_idx;
                  return curr->data[pos - curr_idx];
              } else {
                  curr = head;
                  curr_idx = 0;
              }
          }
      } else {
          curr = head;
          curr_idx = 0;
      }
      
      while (curr && pos >= curr_idx + curr->size) {
          curr_idx += curr->size;
          curr = curr->next;
      }
      
      last_accessed_block = curr;
      last_accessed_idx = curr_idx;
      return curr->data[pos - curr_idx];
  }

  const T &front() const {
      if (total_size == 0) throw container_is_empty();
      return head->data[0];
  }
  const T &back() const {
      if (total_size == 0) throw container_is_empty();
      return tail->data[tail->size - 1];
  }

  iterator begin() {
      if (total_size == 0) return iterator(this, nullptr, 0);
      return iterator(this, head, 0);
  }
  const_iterator cbegin() const {
      if (total_size == 0) return const_iterator(this, nullptr, 0);
      return const_iterator(this, head, 0);
  }

  iterator end() {
      return iterator(this, nullptr, 0);
  }
  const_iterator cend() const {
      return const_iterator(this, nullptr, 0);
  }

  bool empty() const {
      return total_size == 0;
  }

  size_t size() const {
      return total_size;
  }

  void clear() {
      invalidate_cache();
      Block* curr = head;
      while (curr) {
          Block* next = curr->next;
          delete curr;
          curr = next;
      }
      head = tail = nullptr;
      total_size = 0;
  }

  iterator insert(iterator pos, const T &value) {
      invalidate_cache();
      if (pos.q != this) throw invalid_iterator();
      if (pos.b == nullptr) {
          push_back(value);
          return iterator(this, tail, tail->size - 1);
      }
      int idx = get_index(pos);
      Block* b = pos.b;
      size_t p = pos.pos;
      
      if (b->size == BLOCK_CAP) {
          b->split();
          if (b == tail) {
              tail = b->next;
          }
          if (p > b->size) {
              p -= b->size;
              b = b->next;
          }
      }
      
      b->insert(p, value);
      ++total_size;
      
      return begin() + idx;
  }

  iterator erase(iterator pos) {
      invalidate_cache();
      if (pos.q != this || pos.b == nullptr) throw invalid_iterator();
      if (total_size == 0) throw container_is_empty();
      
      int idx = get_index(pos);
      Block* b = pos.b;
      size_t p = pos.pos;
      b->erase(p);
      --total_size;
      
      if (b->size == 0) {
          check_empty(b);
      } else {
          Block* prev = b->prev;
          if (prev) {
              check_merge(prev);
              if (prev->next == b) {
                  check_merge(b);
              }
          } else {
              check_merge(b);
          }
      }
      
      if (idx == total_size) return end();
      return begin() + idx;
  }

  void push_back(const T &value) {
      invalidate_cache();
      if (total_size == 0) {
          if (head == nullptr) {
              head = tail = new Block();
          }
          head->insert(0, value);
      } else {
          if (tail->size == BLOCK_CAP) {
              Block* new_block = new Block();
              tail->next = new_block;
              new_block->prev = tail;
              tail = new_block;
          }
          tail->insert(tail->size, value);
      }
      ++total_size;
  }

  void pop_back() {
      invalidate_cache();
      if (total_size == 0) throw container_is_empty();
      tail->erase(tail->size - 1);
      --total_size;
      if (tail->size == 0) {
          check_empty(tail);
      } else {
          if (tail->prev) check_merge(tail->prev);
      }
  }

  void push_front(const T &value) {
      invalidate_cache();
      if (total_size == 0) {
          if (head == nullptr) {
              head = tail = new Block();
          }
          head->insert(0, value);
      } else {
          if (head->size == BLOCK_CAP) {
              Block* new_block = new Block();
              new_block->next = head;
              head->prev = new_block;
              head = new_block;
          }
          head->insert(0, value);
      }
      ++total_size;
  }

  void pop_front() {
      invalidate_cache();
      if (total_size == 0) throw container_is_empty();
      head->erase(0);
      --total_size;
      if (head->size == 0) {
          check_empty(head);
      } else {
          check_merge(head);
      }
  }
};

} // namespace sjtu

#endif