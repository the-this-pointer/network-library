#ifndef NetLib_POOL_H
#define NetLib_POOL_H

#include <condition_variable>
#include <functional>
#include <vector>
#include <chrono>

namespace thisptr {
  namespace utils {

    template <typename T, typename ...Args>
    class Pool {
    public:
      Pool() = default;
      ~Pool() {
        clear();
      };

      void operator=(const Pool<T>&) = delete;
      void operator=(const Pool<T>&&) = delete;

      Pool(std::function<T* (Args && ...)>&& cFun, std::function<void (T*)>&& dFunc, std::size_t capacity = 1) {
        m_cFunc = std::move(cFun);
        m_dFunc = std::move(dFunc);
        m_cap = capacity;
        m_elems.reserve(capacity);
        /*for (int i = 0; i < capacity; ++i) {
          push(m_cFunc());
        }*/
      }

      void clear() {
        std::lock_guard<std::mutex> lk(m_vecMutex);
        if (!m_dFunc) {
          m_elems.clear();
          return;
        }

        for (auto& elem: m_elems) {
          m_dFunc(elem);
        }
        m_elems.clear();
      }

      bool isEmpty() {
        return !(!m_elems.empty() || (m_popped < m_cap));
      }

      T* pop(int timeout = 0, Args && ...args) {
        using namespace std::chrono_literals;

        {
          std::lock_guard<std::mutex> lk(m_vecMutex);
          if (!m_elems.empty()) {
            T *t = m_elems.back();
            m_elems.pop_back();
            m_popped++;
            return t;
          } else if (m_popped < m_cap) {
            T *t = m_cFunc(std::forward<Args>(args)...);
            m_popped++;
            return t;
          }
        }
        if (timeout > 0) {
          std::unique_lock<std::mutex> lkCv(m_cvMutex);
          if (timeout > 0 && m_cv.wait_for(lkCv, timeout * 1ms, [=]{ return !m_elems.empty(); })) {
            {
              std::lock_guard<std::mutex> lk(m_vecMutex);

              if (!m_elems.empty()) {
                T *t = m_elems.back();
                m_elems.pop_back();
                m_popped++;
                return t;
              } else if (m_popped < m_cap) {
                T *t = m_cFunc(std::forward<Args>(args)...);
                m_popped++;
                return t;
              }
            }
          }
        }
        return nullptr;
      }

      void push(T* elem) {
        std::lock_guard<std::mutex> lk(m_vecMutex);
        if (m_elems.size() < m_cap || !m_dFunc) {
          m_elems.push_back(elem);
          m_popped--;
          m_cv.notify_one();
        } else if (m_dFunc)
          m_dFunc(elem);
      }



    private:
      std::function<T* (Args && ...)> m_cFunc;
      std::function<void (T*)> m_dFunc;
      std::size_t m_cap{};
      int m_popped = 0;

      std::mutex m_vecMutex;
      std::vector<T*> m_elems;

      std::mutex m_cvMutex;
      std::condition_variable m_cv;
    };
  }
}

#endif //NetLib_POOL_H
