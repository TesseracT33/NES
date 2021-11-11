#pragma once

#include <array>
#include <fstream>
#include <queue>
#include <string>
#include <vector>

// Used for save states and configuration files; reads or writes data from/to files
class SerializationStream
{
private:
	std::fstream fstream;

	void Stream(void* obj, size_t size)
	{
		if (mode == Mode::Serialization)
			fstream.write((const char*)obj, size);
		else
			fstream.read((char*)obj, size);

		if (!fstream)
		{
			error = true; // todo; implement better error handling
		}
	}

public:
	enum class Mode { Serialization, Deserialization } const mode;

	bool error = false; // an error has occured while reading/writing.

	SerializationStream(const char* file_path, Mode mode) : SerializationStream(std::string(file_path), mode) {}

	SerializationStream(const std::string& file_path, Mode mode) : mode(mode)
	{
		auto ios_base = mode == Mode::Serialization ?
			std::ios_base::out | std::ios_base::binary :
			std::ios_base::in | std::ios_base::binary;

		fstream = std::fstream{ file_path, ios_base };

		if (!fstream)
		{
			error = true;
		}
	}

	template<typename T> void StreamPrimitive(T& number)
	{
		Stream(&number, sizeof T);
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
		if (mode == Mode::Serialization)
		{
			Stream(&size, sizeof(size_t));
			for (T t : array)
				Stream(&t, sizeof(T));
		}
		else
		{
			size_t new_size = 0;
			Stream(&new_size, sizeof(size_t));
			array = std::array<T, new_size>();
			Stream(&array[0], sizeof(T) * new_size);
		}
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
			while (!queue.empty())
			{
				T t = queue.front();
				queue.pop();
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

	template<typename T> void StreamVector(std::vector<T>& vector)
	{
		if (mode == Mode::Serialization)
		{
			size_t size = vector.size();
			Stream(&size, sizeof(size_t));
			for (T t : vector)
				Stream(&t, sizeof(T));
		}
		else
		{
			vector.clear();

			size_t size = 0;
			Stream(&size, sizeof(size_t));
			if (size > 0)
				vector.resize(size);
			for (size_t i = 0; i < size; i++)
			{
				T t;
				Stream(&t, sizeof(T));
				vector.push_back(t);
			}
		}
	}
};