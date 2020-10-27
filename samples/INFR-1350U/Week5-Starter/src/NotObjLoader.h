#pragma once

#include "MeshBuilder.h"
#include "MeshFactory.h"
#include "IndexBuffer.h"
#include "VertexBuffer.h"
#include "VertexArrayObject.h"

class NotObjLoader
{
public:
	static VertexArrayObject::sptr LoadFromFile(const std::string& filename);

protected:
	NotObjLoader() = default;
	~NotObjLoader() = default;
};