#pragma once

#include <array>
#include <deque>
#include <fstream>
#include <queue>
#include <string>
#include <vector>

/* A class used for save states and configuration files; reads or writes data from/to files */
class SerializationStream
{
private:
	bool has_error = false; // an error has occured while reading/writing.
	std::fstream fstream;

	void Stream(void* obj, const size_t size)
	{
		if (has_error)
			return;

		if (mode == Mode::Serialization)
			fstream.write((const char*)obj, size);
		else
			fstream.read((char*)obj, size);

		if (!fstream)
		{
			has_error = true; // todo; implement better error handling
		}
	}

public:
	enum class Mode { Serialization, Deserialization } const mode;

	SerializationStream(const char* file_path, Mode mode) : SerializationStream(std::string(file_path), mode) {}

	SerializationStream(const std::string& file_path, Mode mode) : mode(mode)
	{
		auto ios_base = mode == Mode::Serialization ?
			std::ios_base::out | std::ios_base::binary :
			std::ios_base::in | std::ios_base::binary;

		fstream = std::fstream{ file_path, ios_base };

		if (!fstream)
		{
			has_error = true;
		}
	}

	SerializationStream(const SerializationStream& other) = delete;

	SerializationStream(SerializationStream&& other) noexcept : mode(other.mode)
	{
		*this = std::move(other);
	}

	SerializationStream& operator=(const SerializationStream& other) = delete;

	SerializationStream& operator=(SerializationStream&& other) noexcept
	{
		if (this != &other)
		{
			this->fstream.swap(other.fstream);
			this->has_error = other.has_error;
			other.has_error = false;
		}
		return *this;
	}

	bool HasError() const
	{
		return has_error;
	}

	template<typename T> void StreamPrimitive(T& number)
	{
		Stream(&number, sizeof T);
	}

	/* We cannot take the address of (pointer/ref) of a bitfield.
	   So take it by copy, read/write from/to it, and then return it.
	   The caller will need to make sure that the result is not discarded. */
	template<typename T> [[nodiscard]] T StreamBitfield(T number)
	{
		Stream(&number, sizeof T);
		return number;
	}

	void StreamString(std::string& str)
	{
		if (mode == Mode::Serialization)
		{
			const char* c_str = str.c_str();
			size_t size = std::strlen(c_str);
			Stream(&size, sizeof(size_t));
			Stream((void*)c_str, size * sizeof(char));
		}
		else
		{
			size_t size = 0;
			Stream(&size, sizeof(size_t));
			char* c_str = new char[size + 1]{};
			Stream(c_str, size * sizeof(char));
			str = std::string(c_str);
			delete[] c_str;
		}
	}

	template<typename T, size_t size> void StreamArray(std::array<T, size>& array)
	{
		Stream(&array[0], sizeof(T) * size);
	}

	template<typename T> void StreamArray(T* array, size_t size)
	{
		Stream(array, sizeof(T) * size);
	}

	template<typename T> void StreamQueue(std::queue<T>& queue)
	{
		if (mode == Mode::Serialization)
		{
			size_t size = queue.size();
			Stream(&size, sizeof(size_t));

			auto tmp_queue = queue;
			while (!tmp_queue.empty())
			{
				T t = tmp_queue.front();
				tmp_queue.pop();
				Stream(&t, sizeof(T));
			}
		}
		else
		{
			while (!queue.empty())
				queue.pop();

			size_t size = 0;
			Stream(&size, sizeof(size_t));
			for (size_t i = 0; i < size; i++)
			{
				T t;
				Stream(&t, sizeof(T));
				queue.push(t);
			}
		}
	}

	template<typename T> void StreamDeque(std::deque<T>& deque)
	{
		if (mode == Mode::Serialization)
		{
			size_t size = deque.size();
			Stream(&size, sizeof(size_t));
			for (T& t : deque)
				Stream(&t, sizeof(T));
		}
		else
		{
			deque.clear();

			size_t size = 0;
			Stream(&size, sizeof(size_t));
			if (size > 0)
				deque.reserve(size);
			for (size_t i = 0; i < size; i++)
			{
				T t;
				Stream(&t, sizeof(T));
				deque.push_back(t);
			}
		}
	}

	template<typename T> void StreamVector(std::vector<T>& vector)
	{
		if (mode == Mode::Serialization)
		{
			size_t size = vector.size();
			Stream(&size, sizeof(size_t));
			for (T& t : vector)
				Stream(&t, sizeof(T));
		}
		else
		{
			vector.clear();

			size_t size = 0;
			Stream(&size, sizeof(size_t));
			if (size > 0)
				vector.reserve(size);
			for (size_t i = 0; i < size; i++)
			{
				T t;
				Stream(&t, sizeof(T));
				vector.push_back(t);
			}
		}
	}
};