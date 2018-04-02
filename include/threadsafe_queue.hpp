#include <memory>
#include <condition_variable>
#include <mutex>

template <typename T>
class threadsafe_queue
{
private:
  struct node_t
  {
    std::shared_ptr<T> data;
    std::unique_ptr<node_t> next;
  };

  std::mutex head_mutex_;
  std::unique_ptr<node_t> head_;
  std::mutex tail_mutex_;
  node_t* tail_;
  std::condition_variable data_cond_;
  
  auto get_tail() -> node_t*;
  auto pop_head() -> std::unique_ptr<node_t>;
  auto wait_for_data() -> std::unique_lock<std::mutex>;
  auto wait_and_pop_head(T& value) -> bool;
  auto wait_and_pop_head() -> std::unique_ptr<node_t>;
public:
  threadsafe_queue();
  threadsafe_queue(const threadsafe_queue& other) = delete;
  auto operator=(const threadsafe_queue& other) -> threadsafe_queue& = delete;
  ~threadsafe_queue() = default;
  
  auto try_pop() -> std::shared_ptr<T>;
  auto try_pop(T& value) -> bool;
  auto wait_and_pop() -> std::shared_ptr<T>;
  auto wait_and_pop(T& value) -> bool;
  auto push(T value) -> void;
  auto empty() -> bool;
};

template <typename T>
threadsafe_queue<T>::threadsafe_queue() : head_{std::make_unique<node_t>()}, tail_{head_.get()}
{
}

template <typename T>
auto threadsafe_queue<T>::push(T value) -> void
{
  auto data = std::make_shared<T>(std::move(value));

  auto node = std::make_unique<node_t>();
  {
    std::lock_guard<std::mutex> tail_lock(tail_mutex_);
    tail_->data = data;
    const auto tail = node.get();
    tail_->next = std::move(node);
    tail_ = tail;
  }
  
  data_cond_.notify_one();
}

template <typename T>
auto threadsafe_queue<T>::get_tail() -> node_t*
{
  std::lock_guard<std::mutex> tail_lock(tail_mutex_);
  return tail_;
}

template <typename T>
auto threadsafe_queue<T>::pop_head() -> std::unique_ptr<node_t> 
{
  auto old_head = std::move(head_);
  head_ = std::move(head_->next);
  return old_head;
}

template <typename T>
auto threadsafe_queue<T>::wait_for_data() -> std::unique_lock<std::mutex>
{
  std::unique_lock<std::mutex> head_lock{head_mutex_};
  data_cond_.wait(head_lock, [&]() {
    return head_.get() != get_tail();
  });

  return head_lock;
}

template <typename T>
auto threadsafe_queue<T>::wait_and_pop_head() -> std::unique_ptr<node_t>
{
  auto head_lock = wait_for_data();
  return pop_head();
}

template <typename T>
auto threadsafe_queue<T>::wait_and_pop_head(T& value) -> bool
{
  auto head_lock = wait_for_data();
  value = std::move(*head_->data);
  return pop_head();
}

template <typename T>
auto threadsafe_queue<T>::wait_and_pop() -> std::shared_ptr<T>
{
  return wait_and_pop_head()->data;
}
 
template <typename T>
auto threadsafe_queue<T>::wait_and_pop(T& value) -> bool
{
  return wait_and_pop_head(value);
}

template <typename T>
auto threadsafe_queue<T>::try_pop() -> std::shared_ptr<T> 
{
  std::lock_guard<std::mutex> head_lock{head_mutex_};
  if ( head_.get() == get_tail()) {
    return nullptr;
  }

  return pop_head()->data;
}

template <typename T>
auto threadsafe_queue<T>::try_pop(T& value) -> bool
{
  std::lock_guard<std::mutex> head_lock{head_mutex_};
  if ( head_.get() == get_tail()) {
    return false;
  }

  value = pop_head();
  return true;
}

template <typename T>
auto threadsafe_queue<T>::empty() -> bool
{
  std::lock_guard<std::mutex> head_lock{head_mutex_};
  return head_.get() == get_tail();
} 
