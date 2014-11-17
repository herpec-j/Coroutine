#pragma once

#include <stdexcept>
#include <thread>
#include <condition_variable>
#include <memory>
#include <cassert>

namespace EffectivePatterns
{
	class NonCopyable
	{
	protected:
		NonCopyable(void) = default;

		NonCopyable(const NonCopyable &) = delete;

		NonCopyable &operator=(const NonCopyable &) = delete;

		virtual ~NonCopyable(void) = default;
	};

	template <typename YieldValue = void>
	class Coroutine : public NonCopyable
	{
	public:
		Coroutine(void) = delete;

		template <typename Func, typename... Args>
		inline Coroutine(const Func &func, Args &&...args)
		{
			std::unique_lock<std::mutex> lock(mutex);
			std::function<void()> coroutine = std::bind(func, this, std::forward<Args>(args)...);
			thread.reset(new std::thread(&Coroutine::run, this, coroutine));
			conditionVariable.wait(lock);
		}

		inline ~Coroutine(void)
		{
			{
				std::lock_guard<std::mutex> lock(mutex);
				quit = true;
				canGetValue = false;
			}
			conditionVariable.notify_all();
			thread->join();
		}

		inline void yield(const YieldValue &yieldValue)
		{
			std::unique_lock<std::mutex> lock(mutex);
			result = yieldValue;
			canGetValue = true;
			conditionVariable.notify_all();
			conditionVariable.wait(lock);
			if (quit)
			{
				throw InterruptedException();
			}
		}

		inline const YieldValue &get(void) const
		{
			assert(canGetValue && "Impossible to get value");
			return result;
		}

		inline const volatile YieldValue &get(void) const volatile
		{
			assert(canGetValue && "Impossible to get value");
			return result;
		}

		inline void resume(void)
		{
			std::unique_lock<std::mutex> lock(mutex);
			assert(!quit && "Invalid coroutine");
			if (!quit)
			{
				conditionVariable.notify_all();
				conditionVariable.wait(lock);
			}
		}

		inline operator bool(void) const
		{
			return !quit;
		}

		inline operator bool(void) const volatile
		{
			return !quit;
		}

	private:
		class InterruptedException : public std::exception
		{

		};

		std::unique_ptr<std::thread> thread;
		std::condition_variable conditionVariable;
		std::mutex mutex;
		bool quit = false;
		bool canGetValue = false;
		YieldValue result;

		inline void run(const std::function<void()> &coroutine)
		{
			try
			{
				coroutine();
			}
			catch (const InterruptedException &)
			{
				return;
			}
			std::lock_guard<std::mutex> lock(mutex);
			quit = true;
			canGetValue = false;
			conditionVariable.notify_all();
		}
	};

	template <>
	class Coroutine<void> : public NonCopyable
	{
	public:
		Coroutine(void) = delete;

		template <typename Func, typename... Args>
		inline Coroutine(const Func &func, Args &&...args)
		{
			std::unique_lock<std::mutex> lock(mutex);
			std::function<void()> coroutine = std::bind(func, this, std::forward<Args>(args)...);
			thread.reset(new std::thread(&Coroutine::run, this, coroutine));
			conditionVariable.wait(lock);
		}

		inline ~Coroutine(void)
		{
			{
				std::lock_guard<std::mutex> lock(mutex);
				quit = true;
			}
			conditionVariable.notify_all();
			thread->join();
		}

		inline void yield(void)
		{
			std::unique_lock<std::mutex> lock(mutex);
			conditionVariable.notify_all();
			conditionVariable.wait(lock);
			if (quit)
			{
				throw InterruptedException();
			}
		}

		inline void resume(void)
		{
			std::unique_lock<std::mutex> lock(mutex);
			assert(!quit && "Invalid coroutine");
			if (!quit)
			{
				conditionVariable.notify_all();
				conditionVariable.wait(lock);
			}
		}

		inline operator bool(void) const
		{
			return !quit;
		}

		inline operator bool(void) const volatile
		{
			return !quit;
		}

	private:
		class InterruptedException : public std::exception
		{

		};

		std::unique_ptr<std::thread> thread;
		std::condition_variable conditionVariable;
		std::mutex mutex;
		bool quit = false;

		inline void run(const std::function<void()> &coroutine)
		{
			try
			{
				coroutine();
			}
			catch (const InterruptedException &)
			{
				return;
			}
			std::lock_guard<std::mutex> lock(mutex);
			quit = true;
			conditionVariable.notify_all();
		}
	};
}