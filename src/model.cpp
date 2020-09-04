#include <grend/gameModel.hpp>
#include <grend/utility.hpp>
#include <grend/scene.hpp>

#include <tinygltf/stb_image.h>
#include <tinygltf/stb_image_write.h>

#include <vector>
#include <map>
#include <string>
#include <exception>
#include <iostream>
#include <fstream>
#include <sstream>

#include <stdint.h>

namespace grendx {

void gameModel::genNormals(void) {
	std::cerr << " > generating new normals... " << vertices.size() << std::endl;
	normals.resize(vertices.size(), glm::vec3(0));

	for (auto& [name, ptr] : nodes) {
		if (ptr->type != gameObject::objType::Mesh) {
			continue;
		}
		gameMesh::ptr mesh = std::dynamic_pointer_cast<gameMesh>(ptr);

		for (unsigned i = 0; i < mesh->faces.size(); i += 3) {
			// TODO: bounds check
			GLuint elms[3] = {
				mesh->faces[i],
				mesh->faces[i+1],
				mesh->faces[i+2]
			};

			glm::vec3 normal = glm::normalize(
					glm::cross(
						vertices[elms[1]] - vertices[elms[0]],
						vertices[elms[2]] - vertices[elms[0]]));

			normals[elms[0]] = normals[elms[1]] = normals[elms[2]] = normal;
		}
	}
}

void gameModel::genTexcoords(void) {
	std::cerr << " > generating new texcoords... " << vertices.size() << std::endl;
	texcoords.resize(vertices.size());

	for (auto& [name, ptr] : nodes) {
		if (ptr->type != gameObject::objType::Mesh) {
			continue;
		}
		gameMesh::ptr mesh = std::dynamic_pointer_cast<gameMesh>(ptr);

		for (unsigned i = 0; i < mesh->faces.size(); i++) {
			// TODO: bounds check
			GLuint elm = mesh->faces[i];

			glm::vec3& foo = vertices[elm];
			texcoords[elm] = {foo.x, foo.y};
		}
	}
}

void gameModel::genTangents(void) {
	std::cerr << " > generating tangents... " << vertices.size() << std::endl;
	// allocate a little bit extra to make sure we don't overrun the buffer...
	// XXX:
	// TODO: just realized, this should be working with faces, not vertices
	//       so fix this after gltf stuff is in a workable state
	//unsigned mod = 3 - vertices.size()%3;
	unsigned mod = 3;

	tangents.resize(vertices.size(), glm::vec3(0));
	bitangents.resize(vertices.size(), glm::vec3(0));

	// generate tangents for each triangle
	for (auto& [name, ptr] : nodes) {
		if (ptr->type != gameObject::objType::Mesh) {
			continue;
		}
		gameMesh::ptr mesh = std::dynamic_pointer_cast<gameMesh>(ptr);

		for (std::size_t i = 0; i+2 < mesh->faces.size(); i += 3) {
			// TODO: bounds check
			GLuint elms[3] = {mesh->faces[i], mesh->faces[i+1], mesh->faces[i+2]};

			glm::vec3& a = vertices[elms[0]];
			glm::vec3& b = vertices[elms[1]];
			glm::vec3& c = vertices[elms[2]];

			glm::vec2 auv = texcoords[elms[0]];
			glm::vec2 buv = texcoords[elms[1]];
			glm::vec2 cuv = texcoords[elms[2]];

			glm::vec3 e1 = b - a, e2 = c - a;
			glm::vec2 duv1 = buv - auv, duv2 = cuv - auv;

			glm::vec3 tangent, bitangent;

			float f = 1.f / (duv1.x * duv2.y - duv2.x * duv1.y);
			tangent.x = f * (duv2.y * e1.x + duv1.y * e2.x);
			tangent.y = f * (duv2.y * e1.y + duv1.y * e2.y);
			tangent.z = f * (duv2.y * e1.z + duv1.y * e2.z);

			bitangent.x = f * (-duv2.x * e1.x + duv1.x * e2.x);
			bitangent.y = f * (-duv2.x * e1.y + duv1.x * e2.y);
			bitangent.z = f * (-duv2.x * e1.z + duv1.x * e2.z);

			tangents[elms[0]] = tangents[elms[1]] = tangents[elms[2]]
				= glm::normalize(tangent);
			bitangents[elms[0]] = bitangents[elms[1]] = bitangents[elms[2]]
				= glm::normalize(bitangent);
		}
	}
}

static std::string base_dir(std::string filename) {
	std::size_t found = filename.rfind("/");

	if (found != std::string::npos) {
		return filename.substr(0, found) + "/";
	}

	return filename;
}

gameModel::ptr load_object(std::string filename) {
	std::cerr << " > loading " << filename << std::endl;
	std::ifstream input(filename);
	std::string line;
	std::string mesh_name = "default";
	std::string mesh_material = "(null)";
	std::string current_mesh_name = mesh_name + ":" + mesh_material;
	gameMesh::ptr current_mesh = nullptr;

	std::vector<glm::vec3> vertbuf = {};
	std::vector<glm::vec3> normbuf = {};
	std::vector<glm::vec2> texbuf = {};

	if (!input.good()) {
		// TODO: exception
		std::cerr << " ! couldn't load object from " << filename << std::endl;
		return nullptr;
	}

	gameModel::ptr ret = gameModel::ptr(new gameModel());
	ret->sourceFile = filename;

	// TODO: split this into a seperate parse function
	while (std::getline(input, line)) {
		auto statement = split_string(line);

		if (statement.size() < 2)
			continue;

		if (statement[0] == "o") {
			std::cerr << " > have submesh " << statement[1] << std::endl;
			// TODO: should current_mesh just be defined at the top
			//       of this loop?
			mesh_name = statement[1];
			current_mesh_name = mesh_name + ":(null)";
			current_mesh = gameMesh::ptr(new gameMesh());
			setNode(current_mesh_name, ret, current_mesh);
		}

		else if (statement[0] == "mtllib") {
			std::string temp = base_dir(filename) + statement[1];
			std::cerr << " > using material " << temp << std::endl;
			load_materials(ret, temp);
		}

		else if (statement[0] == "usemtl") {
			std::cerr << " > using material " << statement[1] << std::endl;
			current_mesh = gameMesh::ptr(new gameMesh());
			current_mesh->material = statement[1];
			current_mesh_name = mesh_name + ":" + current_mesh->material;

			setNode(current_mesh_name, ret, current_mesh);
		}

		else if (statement[0] == "v") {
			glm::vec3 v = glm::vec3(std::stof(statement[1]),
			                        std::stof(statement[2]),
			                        std::stof(statement[3]));
			vertbuf.push_back(v);
		}

		else if (statement[0] == "f") {
			std::size_t end = statement.size();

			// XXX: we should be checking for buffer ranges here
			auto load_face_tri = [&] (std::string& statement) {
				auto spec = split_string(statement, '/');
				unsigned vert_index = std::stoi(spec[0]) - 1;

				ret->vertices.push_back(vertbuf[vert_index]);
				current_mesh->faces.push_back(ret->vertices.size() - 1);

				if (ret->haveTexcoords && spec.size() > 1 && !spec[1].empty()) {
					unsigned buf_index = std::stoi(spec[1]) - 1;
					ret->texcoords.push_back(texbuf[buf_index]);
				}

				if (ret->haveNormals && spec.size() > 2 && !spec[2].empty()) {
					ret->normals.push_back(normbuf[std::stoi(spec[2]) - 1]);
				}
			};

			for (std::size_t cur = 1; cur + 2 < end; cur++) {
				load_face_tri(statement[1]);

				for (unsigned k = 1; k < 3; k++) {
					load_face_tri(statement[cur + k]);
				}
			}
		}

		else if (statement[0] == "vn") {
			glm::vec3 v = glm::vec3(std::stof(statement[1]),
			                        std::stof(statement[2]),
			                        std::stof(statement[3]));
			//normals.push_back(glm::normalize(v));
			normbuf.push_back(v);
			ret->haveNormals = true;
		}

		else if (statement[0] == "vt") {
			texbuf.push_back({std::stof(statement[1]), std::stof(statement[2])});
			ret->haveTexcoords = true;
		}

	}

	for (const auto& [name, ptr] : ret->nodes) {
		std::cerr << " > > have mesh node " << name << std::endl; 
	}

	// TODO: check that normals size == vertices size and fill in the difference
	if (!ret->haveNormals) {
		ret->genNormals();
	}

	if (!ret->haveTexcoords) {
		ret->genTexcoords();
	}

	ret->genTangents();

	if (ret->normals.size() != ret->vertices.size()) {
		std::cerr << " ? mismatched normals and vertices: "
			<< ret->normals.size() << ", "
			<< ret->vertices.size()
			<< std::endl;
		// TODO: should handle this
	}

	if (ret->texcoords.size() != ret->vertices.size()) {
		std::cerr << " ? mismatched texcoords and vertices: "
			<< ret->texcoords.size() << ", "
			<< ret->vertices.size()
			<< std::endl;
		// TODO: should handle this
	}

	if (ret->tangents.size() != ret->vertices.size()
	    || ret->bitangents.size() != ret->vertices.size())
	{
		std::cerr << " ? mismatched tangents and vertices: "
			<< ret->tangents.size() << ", "
			<< ret->vertices.size()
			<< std::endl;
	}

	return ret;
}

materialTexture::materialTexture(std::string filename) {
	load_texture(filename);
}

void materialTexture::load_texture(std::string filename) {
	std::cerr << "loading " << filename << std::endl;
	uint8_t *datas = stbi_load(filename.c_str(), &width,
	                           &height, &channels, 0);

	if (!datas) {
		throw std::logic_error("Couldn't load material texture " + filename);
	}

	size_t imgsize = width * height * channels;
	//pixels.insert(pixels.end(), datas, datas + imgsize);
	for (unsigned i = 0; i < imgsize; i++) {
		pixels.push_back(datas[i]);
	}

	stbi_image_free(datas);
}

void load_materials(gameModel::ptr model, std::string filename) {
	std::ifstream input(filename);
	std::string current_material = "default";
	std::string line;

	if (!input.good()) {
		// TODO: exception
		std::cerr << "Warning: couldn't load material library from "
			<< filename << std::endl;
		return;
	}

	stbi_set_flip_vertically_on_load(true);

	while (std::getline(input, line)) {
		auto statement = split_string(line);

		if (statement.size() < 2) {
			continue;
		}

		if (statement[0] == "newmtl") {
			std::cerr << "   - new material: " << statement[1] << std::endl;
			current_material = statement[1];
		}

		else if (statement[0] == "Ka") {
			model->materials[current_material].ambient =
				glm::vec4(std::stof(statement[1]),
			              std::stof(statement[2]),
			              std::stof(statement[3]), 1);
		}

		else if (statement[0] == "Kd") {
			model->materials[current_material].diffuse =
				glm::vec4(std::stof(statement[1]),
			              std::stof(statement[2]),
			              std::stof(statement[3]), 1);

		}

		else if (statement[0] == "Ks") {
			model->materials[current_material].specular =
				glm::vec4(std::stof(statement[1]),
			             std::stof(statement[2]),
			             std::stof(statement[3]), 1);
		}

		else if (statement[0] == "d") {
			model->materials[current_material].opacity
				= std::stof(statement[1]);
		}

		else if (statement[0] == "Ns") {
			model->materials[current_material].roughness =
				1.f - std::stof(statement[1])/1000.f;
		}

		else if (statement[0] == "illum") {
			unsigned illumination_model = std::stoi(statement[1]);

			switch (illumination_model) {
				case 4:
				case 6:
				case 7:
				case 9:
					model->materials[current_material].blend
						= material::blend_mode::Blend;
					break;

				default:
					model->materials[current_material].blend
						= material::blend_mode::Opaque;
					break;
			}
		}

		else if (statement[0] == "map_Kd") {
			//materials[current_material].diffuse_map = base_dir(filename) + statement[1];
			model->materials[current_material].diffuse_map.
				load_texture(base_dir(filename) + statement[1]);
		}

		else if (statement[0] == "map_Ns") {
			// specular map
			//materials[current_material].specular_map = base_dir(filename) + statement[1];
			model->materials[current_material].metal_roughness_map.
				load_texture(base_dir(filename) + statement[1]);
		}

		else if (statement[0] == "map_ao") {
			// ambient occlusion map (my own extension)
			//materials[current_material].ambient_occ_map = base_dir(filename) + statement[1];
			model->materials[current_material].ambient_occ_map.
				load_texture(base_dir(filename) + statement[1]);
		}

		else if (statement[0] == "map_norm" || statement[0] == "norm") {
			// normal map (also non-standard)
			//materials[current_material].normal_map = base_dir(filename) + statement[1];
			model->materials[current_material].normal_map.
				load_texture(base_dir(filename) + statement[1]);
		}

		else if (statement[0] == "map_bump") {
			// bump/height map
		}

		// TODO: other light maps, attributes
	}

	stbi_set_flip_vertically_on_load(false);
}

// namespace grendx
}

template <typename T>
static inline bool check_index(std::vector<T> container, size_t idx) {
	if (idx >= container.size()) {
		// TODO: toggle exceptions somehow
		throw std::out_of_range("check_index()");
		return false;
	}

	return true;
}

static inline bool assert_type(int type, int expected) {
	if (type != expected) {
		throw std::invalid_argument(
			"assert_type(): have " + std::to_string(type)
			+ ", expected " + std::to_string(expected));
		return false;
	}

	return true;
}

static inline bool assert_nonzero(size_t x) {
	if (x == 0) {
		throw std::logic_error("assert_nonzero()");
		return false;
	}

	return true;
}

static tinygltf::Material& gltf_material(tinygltf::Model& gltf_model, size_t idx) {
	check_index(gltf_model.materials, idx);
	return gltf_model.materials[idx];
}

static tinygltf::Texture& gltf_texture(tinygltf::Model& gltf_model, size_t idx) {
	check_index(gltf_model.textures, idx);
	return gltf_model.textures[idx];
}

static tinygltf::Image& gltf_image(tinygltf::Model& gltf_model, size_t idx) {
	check_index(gltf_model.textures, idx);
	return gltf_model.images[idx];
}

static size_t gltf_buff_element_size(int component, int type) {
	size_t size_component = 0, size_type = 0;

	switch (type) {
		case TINYGLTF_TYPE_VECTOR:
		case TINYGLTF_TYPE_SCALAR: size_type = 1; break;
		case TINYGLTF_TYPE_VEC2: size_type = 2; break;
		case TINYGLTF_TYPE_VEC3: size_type = 3; break;
		case TINYGLTF_TYPE_VEC4: size_type = 4; break;
		case TINYGLTF_TYPE_MATRIX: size_type = 1; /* TODO: check this */ break;
		case TINYGLTF_TYPE_MAT2: size_type = 2*2; break;
		case TINYGLTF_TYPE_MAT3: size_type = 3*3; break;
		case TINYGLTF_TYPE_MAT4: size_type = 4*4; break;
	}

	switch (component) {
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		case TINYGLTF_COMPONENT_TYPE_BYTE:
			size_component = sizeof(GLbyte);
			break;

		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		case TINYGLTF_COMPONENT_TYPE_SHORT:
			size_component = sizeof(GLushort);
			break;

		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		case TINYGLTF_COMPONENT_TYPE_INT:
			size_component = sizeof(GLint);
			break;

		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			size_component = sizeof(GLfloat);
			break;

		case TINYGLTF_COMPONENT_TYPE_DOUBLE:
			size_component = sizeof(GLdouble); 
			break;
	}

	return size_component * size_type;
}

static void *gltf_load_accessor(tinygltf::Model& tgltf_model, int idx) {
	check_index(tgltf_model.accessors, idx);
	auto& acc = tgltf_model.accessors[idx];

	size_t elem_size =
		gltf_buff_element_size(acc.componentType, acc.type);
	size_t buff_size = acc.count * elem_size;
	assert_nonzero(elem_size);

	check_index(tgltf_model.bufferViews, acc.bufferView);
	auto& view = tgltf_model.bufferViews[acc.bufferView];
	check_index(tgltf_model.buffers, view.buffer);
	auto& buffer = tgltf_model.buffers[view.buffer];

	std::cerr << "        - acc @ " << idx << std::endl;
	std::cerr << "        - view @ " << acc.bufferView << std::endl;
	std::cerr << "        - buffer @ " << view.buffer << std::endl;

	check_index(buffer.data,
			view.byteOffset + acc.byteOffset + buff_size - 1);
	return static_cast<void*>(buffer.data.data() + (view.byteOffset + acc.byteOffset));
};

template<typename T>
static void gltf_unpack_buffer(tinygltf::Model& gltf_model,
                               int accessor,
                               std::vector<T>& vec)
{
	check_index(gltf_model.accessors, accessor);
	auto& acc = gltf_model.accessors[accessor];
	check_index(gltf_model.bufferViews, acc.bufferView);
	auto& view = gltf_model.bufferViews[acc.bufferView];

	uint8_t *datas = static_cast<uint8_t*>(gltf_load_accessor(gltf_model, accessor));
	size_t emsz = view.byteStride
		? view.byteStride
		: gltf_buff_element_size(acc.componentType, acc.type);

	for (unsigned i = 0; i < acc.count; i++) {
		T *em = reinterpret_cast<T*>(datas + i*emsz);
		vec.push_back(*em);
	}
}

static void gltf_unpack_ushort_to_uint(tinygltf::Model& gltf_model,
                                       int accessor,
                                       std::vector<GLuint>& vec)
{
	check_index(gltf_model.accessors, accessor);
	auto& acc = gltf_model.accessors[accessor];
	check_index(gltf_model.bufferViews, acc.bufferView);
	auto& view = gltf_model.bufferViews[acc.bufferView];

	uint8_t *datas = static_cast<uint8_t*>(gltf_load_accessor(gltf_model, accessor));
	size_t emsz = view.byteStride
		? view.byteStride
		: gltf_buff_element_size(acc.componentType, acc.type);

	for (unsigned i = 0; i < acc.count; i++) {
		//T *em = reinterpret_cast<T*>(datas + i*emsz);
		GLushort *em = reinterpret_cast<GLushort*>(datas + i*emsz);
		vec.push_back((GLuint)*em);
	}
}

static grendx::materialTexture
gltf_load_texture(tinygltf::Model& gltf_model, int tex_idx) {
	grendx::materialTexture ret;

	auto& tex = gltf_texture(gltf_model, tex_idx);
	std::cerr << "        + texture source: " << tex.source << std::endl;

	auto& img = gltf_image(gltf_model, tex.source);
	std::cerr << "        + texture image source: " << img.uri << ", "
		<< img.width << "x" << img.height << ":" << img.component << std::endl;


	ret.pixels.insert(ret.pixels.end(),
			img.image.begin(), img.image.end());
	ret.width = img.width;
	ret.height = img.height;
	ret.channels = img.component;
	ret.size = ret.width * ret.height * ret.channels;

	return ret;
}

static void gltf_load_material(tinygltf::Model& gltf_model,
                               grendx::gameModel::ptr out_model,
                               int material_idx,
                               std::string mesh_name)
{
	// TODO: maybe should pass sub-mesh name as an argument
	std::string temp_name = mesh_name + ":" + std::to_string(material_idx);
	struct grendx::material mod_mat;
	auto& mat = gltf_material(gltf_model, material_idx);

	assert(out_model->hasNode(temp_name));
	grendx::gameMesh::ptr mesh
		= std::dynamic_pointer_cast<grendx::gameMesh>(out_model->nodes[temp_name]);
	std::string matidxname = mat.name + ":" + std::to_string(material_idx);
	mesh->material = matidxname;

	auto& pbr = mat.pbrMetallicRoughness;
	auto& base_color = pbr.baseColorFactor;
	glm::vec4 mat_diffuse(base_color[0], base_color[1],
			base_color[2], base_color[3]);

	std::cerr << "        + have material " << matidxname << std::endl;
	std::cerr << "        + base color: "
		<< mat_diffuse.x << ", " << mat_diffuse.y
		<< ", " << mat_diffuse.z << ", " << mat_diffuse.w
		<< std::endl;
	std::cerr << "        + alphaMode: " << mat.alphaMode << std::endl;
	std::cerr << "        + alphaCutoff: " << mat.alphaCutoff << std::endl;

	std::cerr << "        + pbr roughness: "
		<< pbr.roughnessFactor << std::endl;

	std::cerr << "        + pbr metallic: "
		<< pbr.metallicFactor << std::endl;

	std::cerr << "        + pbr base idx: "
		<< pbr.baseColorTexture.index << std::endl;
	std::cerr << "        + metal/roughness map idx: "
		<< pbr.metallicRoughnessTexture.index << std::endl;
	std::cerr << "        + normal map idx: "
		<< mat.normalTexture.index << std::endl;
	std::cerr << "        + occlusion map idx: "
		<< mat.occlusionTexture.index << std::endl;

	if (pbr.baseColorTexture.index >= 0) {
		mod_mat.diffuse_map =
			gltf_load_texture(gltf_model, pbr.baseColorTexture.index);
	}

	if (pbr.metallicRoughnessTexture.index >= 0) {
		mod_mat.metal_roughness_map =
			gltf_load_texture(gltf_model, pbr.metallicRoughnessTexture.index);
	}


	if (mat.normalTexture.index >= 0) {
		mod_mat.normal_map =
			gltf_load_texture(gltf_model, mat.normalTexture.index);
	}

	if (mat.occlusionTexture.index >= 0) {
		mod_mat.ambient_occ_map =
			gltf_load_texture(gltf_model, mat.occlusionTexture.index);
	}

	if (mat.alphaMode == "BLEND") {
		// XXX:
		mod_mat.opacity = mat.alphaCutoff;
		mod_mat.blend = grendx::material::blend_mode::Blend;
	}

	mod_mat.roughness = pbr.roughnessFactor;
	mod_mat.diffuse = mat_diffuse;
	out_model->materials[matidxname] = mod_mat;
}

grendx::model_map grendx::load_gltf_models(tinygltf::Model& tgltf_model) {
	model_map ret;

	for (auto& mesh : tgltf_model.meshes) {
		grendx::gameModel::ptr curModel =
			grendx::gameModel::ptr(new grendx::gameModel());
		ret[mesh.name] = curModel;

		std::cerr << " GLTF > have mesh " << mesh.name << std::endl;
		std::cerr << "      > primitives: " << std::endl;

		for (size_t i = 0; i < mesh.primitives.size(); i++) {
			auto& prim = mesh.primitives[i];
			std::string suffix = ":[p"+std::to_string(i)+"]";
			std::string temp_name = mesh.name + suffix
				+ ":" + std::to_string(prim.material);

			std::cerr << "        primitive: " << temp_name << std::endl;
			std::cerr << "        material: " << prim.material << std::endl;
			std::cerr << "        indices: " << prim.indices << std::endl;
			std::cerr << "        mode: " << prim.mode << std::endl;

			grendx::gameMesh::ptr modmesh = grendx::gameMesh::ptr(new grendx::gameMesh());
			setNode(temp_name, curModel, modmesh);

			if (prim.material >= 0) {
				gltf_load_material(tgltf_model, curModel,
				                   prim.material, mesh.name + suffix);

			} else {
				modmesh->material = "(null)";
				//ret[mesh.name].meshes[temp_name].material = "(null)";
			}

			// accessor indices
			int elements = prim.indices;
			int normals = -1;
			int position = -1;
			int texcoord = -1;

			for (auto& attr : prim.attributes) {
				if (attr.first == "NORMAL") normals = attr.second;
				if (attr.first == "POSITION") position = attr.second;
				if (attr.first == "TEXCOORD_0") texcoord = attr.second;
			}

			if (elements >= 0) {
				check_index(tgltf_model.accessors, elements);

				auto& acc = tgltf_model.accessors[elements];
				assert_type(acc.type, TINYGLTF_TYPE_SCALAR);
				/*
				assert_type(acc.componentType, 
					TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
					*/

				size_t vsize = curModel->vertices.size();
				auto& submesh = modmesh->faces;
				//gltf_unpack_buffer(tgltf_model, elements, submesh);
				if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
					gltf_unpack_ushort_to_uint(tgltf_model, elements, submesh);
				} else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					gltf_unpack_buffer(tgltf_model, elements, submesh);
				} else {
					assert_type(acc.componentType, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);
				}

				// adjust element indices for vertices already in the model
				// (model element indices are per-model, seems gltf is per-primitive)
				for (auto& face : submesh) {
					face += vsize;
				}
			}

			if (normals >= 0) {
				check_index(tgltf_model.accessors, normals);

				gltf_unpack_buffer(tgltf_model, normals, curModel->normals);
				curModel->haveNormals = true;
			}

			if (position >= 0) {
				check_index(tgltf_model.accessors, position);

				auto& acc = tgltf_model.accessors[position];
				assert_type(acc.type, TINYGLTF_TYPE_VEC3);
				assert_type(acc.componentType, 
					TINYGLTF_COMPONENT_TYPE_FLOAT);

				gltf_unpack_buffer(tgltf_model, position, curModel->vertices);
			}

			if (texcoord >= 0) {
				check_index(tgltf_model.accessors, texcoord);

				auto& acc = tgltf_model.accessors[texcoord];
				assert_type(acc.type, TINYGLTF_TYPE_VEC2);
				assert_type(acc.componentType, 
					TINYGLTF_COMPONENT_TYPE_FLOAT);

				gltf_unpack_buffer(tgltf_model, texcoord, curModel->texcoords);
				curModel->haveTexcoords = true;
			}
		}

		if (!curModel->haveNormals) {
			curModel->genNormals();
		}

		if (!curModel->haveTexcoords) {
			curModel->genTexcoords();
		}

		// TODO: parse tangents from gltf, if available
		curModel->genTangents();
	}

	return ret;
}

static grendx::scene load_gltf_scene_nodes(tinygltf::Model& gmod) {
	grendx::scene ret;

	for (auto& scene : gmod.scenes) {
		for (int nodeidx : scene.nodes) {
			glm::quat rotation(1, 0, 0, 0);
			glm::vec3 translation = {0, 0, 0};
			glm::vec3 scale = {1, 1, 1};

			auto& node = gmod.nodes[nodeidx];
			// no mesh, don't load this as an object
			if (node.mesh < 0) continue;

			auto& mesh = gmod.meshes[node.mesh];
			glm::mat4 mat;
			bool inverted = false;

			if (node.rotation.size() == 4)
				rotation = glm::make_quat(node.rotation.data());
			if (node.translation.size() == 3)
				translation = glm::make_vec3(node.translation.data());

			if (node.scale.size() == 3) {
				scale = glm::make_vec3(node.scale.data());
			}

			/*
			// TODO: if no T/R/S specifiers, show an error/warning,
			//       extract info from the matrix
			if (node.matrix.size() == 16) {
				mat = glm::make_mat4(node.matrix.data());
			}
			*/

			ret.nodes.push_back({
				mesh.name,
				translation,
				scale,
				rotation,
				// TODO: does gltf specify the face order somewhere?
			});
		}
	}

	return ret;
}

static tinygltf::Model open_gltf_model(std::string filename) {
	// TODO: parse extension, handle binary format
	tinygltf::TinyGLTF loader;
	tinygltf::Model tgltf_model;

	std::string err;
	std::string warn;

	bool loaded = loader.LoadASCIIFromFile(&tgltf_model, &err, &warn, filename);

	if (!loaded) {
		throw std::logic_error(err);
	}

	if (!err.empty()) {
		std::cerr << "/!\\ ERROR: " << err << std::endl;
	}

	if (!warn.empty()) {
		std::cerr << "/!\\ WARNING: " << warn << std::endl;
	}

	return tgltf_model;
}

static void updateModelSources(grendx::model_map& models, std::string filename) {
	for (auto& [name, model] : models) {
		model->sourceFile = filename;
	}
}

grendx::model_map grendx::load_gltf_models(std::string filename) {
	tinygltf::Model gmod = open_gltf_model(filename);
	auto models = load_gltf_models(gmod);
	std::cerr << " GLTF > loaded a thing successfully" << std::endl;

	updateModelSources(models, filename);
	return models;
}

std::pair<grendx::scene, grendx::model_map>
grendx::load_gltf_scene(std::string filename) {
	tinygltf::Model gmod = open_gltf_model(filename);
	grendx::scene scene = load_gltf_scene_nodes(gmod);
	grendx::model_map models = load_gltf_models(gmod);

	updateModelSources(models, filename);
	return {scene, models};
}
