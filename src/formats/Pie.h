/*
	Copyright 2010 Warzone 2100 Project

	This file is part of WMIT.

	WMIT is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	WMIT is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with WMIT.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef PIE_HPP
#define PIE_HPP

#include <iostream>
#include <vector>
#include <bitset>
#include <type_traits>
#include <map>
#include <list>
#include <GL/glew.h>
#include "VectorTypes.h"
#include "Polygon.h"

#include "WZM.h" // for friends

/**
  * Templates used to remove tedious code
  * duplication.
  *
  * See this doc for more details:
  * https://github.com/Warzone2100/warzone2100/blob/master/doc/PIE.md
  */

#define PIE_MODEL_SIGNATURE "PIE"
#define PIE_MODEL_DIRECTIVE_TYPE "TYPE"
#define PIE_MODEL_DIRECTIVE_TEXTURE "TEXTURE"
#define PIE_MODEL_DIRECTIVE_NORMALMAP "NORMALMAP"
#define PIE_MODEL_DIRECTIVE_MATERIALS "MATERIALS" // WZ 3.2 only
#define PIE_MODEL_DIRECTIVE_LEVELS "LEVELS"
#define PIE_MODEL_DIRECTIVE_CONNECTORS "CONNECTORS"
#define PIE_MODEL_DIRECTIVE_SPECULARMAP "SPECULARMAP"
#define PIE_MODEL_DIRECTIVE_SHADERS "SHADERS"
#define PIE_MODEL_DIRECTIVE_EVENT "EVENT" // WZ 3.3
#define PIE_MODEL_DIRECTIVE_ANIMOBJECT "ANIMOBJECT" // WZ 3.3
#define PIE_MODEL_DIRECTIVE_NORMALS "NORMALS" // WZ after 3.3 (TBD)

#define PIE_MODEL_FEATURE_TEXTURED 0x200
#define PIE_MODEL_FEATURE_TCMASK 0x10000

#define PIE_MODEL_TEXPAGE_PREFIX "page-"
#define PIE_MODEL_TCMASK_SUFFIX "_tcmask"

// Saw on https://stackoverflow.com/questions/17350214/using-enum-class-with-stdbitset

template<typename T>
struct EnumTraits;

template<typename T>
class EnumClassBitset
{
private:
	std::bitset<static_cast<typename std::underlying_type<T>::type>(EnumTraits<T>::max)> c;

	typename std::underlying_type<T>::type get_value(T v) const
	{
		return static_cast<typename std::underlying_type<T>::type>(v);
	}

public:
	EnumClassBitset() = default;
	EnumClassBitset(const std::string& bit_string): c(bit_string) {}

	bool test(T pos) const
	{
		const typename std::underlying_type<T>::type val(get_value(pos));
		return c.test(val);
	}

	void set(T pos, bool val = true)
	{
		c.set(get_value(pos), val);
	}

	EnumClassBitset& reset()
	{
		c.reset();
		return *this;
	}

	EnumClassBitset& reset(T pos)
	{
		c.reset(get_value(pos));
		return *this;
	}

	EnumClassBitset& flip(T pos)
	{
		c.flip(get_value(pos));
		return *this;
	}

	size_t size() const {return c.size();}
};

enum class PIE_OPT_DIRECTIVES
{
	podNORMALMAP,
	podSPECULARMAP,
	podEVENT,
	podMATERIALS,
	podSHADERS,

	podNORMALS,
	podCONNECTORS,
	podANIMOBJECT,
	pod_MAXVAL
};

const char* getPieDirectiveName(PIE_OPT_DIRECTIVES dir);
const char* getPieDirectiveDescription(PIE_OPT_DIRECTIVES dir);

bool tryToReadDirective(std::istream &in, const char* directive, const bool isOptional,
			std::function<bool(std::istream&)> dirLoaderFunc);

template<>
struct EnumTraits<PIE_OPT_DIRECTIVES>
{
	static const PIE_OPT_DIRECTIVES max = PIE_OPT_DIRECTIVES::pod_MAXVAL;
};

typedef EnumClassBitset<PIE_OPT_DIRECTIVES> PieCaps;

// Note that string bits are reversed!
const static PieCaps PIE2_CAPS("11010111");
const static PieCaps PIE3_CAPS("11010111");

class ApieAnimFrame
{
public:
	int num;
	Vertex<int> pos, rot;
	Vertex<float> scale;

	bool read(std::istream& in);
	void write(std::ostream& out) const;
};

class ApieAnimObject
{
public:
	int time, cycles, numframes;
	std::vector<ApieAnimFrame> frames;

	bool isValid() const {return !frames.empty();}
	void clear() {frames.clear();}

	bool read(std::istream& in);
	void write(std::ostream& out) const;

	bool readStandaloneAniFile(const char* file);
};

template<typename V, typename P, typename C>
class APieLevel
{
	typedef Vertex<GLfloat> PieNormal;
	friend Mesh::operator Pie3Level() const;
	friend Mesh::Mesh(const Pie3Level& p3);
public:
	APieLevel();
	virtual ~APieLevel() {}

	virtual bool read(std::istream& in, PieCaps& caps);
	virtual void write(std::ostream& out, const PieCaps& caps) const;

	size_t points() const;
	size_t normals() const;
	size_t polygons() const;
	size_t connectors() const;

	bool isValid() const;

protected:
	void clearAll();
	bool readAnimObjectDirective(std::istream &in, PieCaps& caps);

	std::vector<V> m_points;
	std::vector<PieNormal> m_normals;
	std::vector<P> m_polygons;
	std::list<C> m_connectors;
	WZMaterial m_material; // PIE3+
	std::string m_shader_vert;
	std::string m_shader_frag;
	ApieAnimObject m_animobj;
};

template <typename L>
class APieModel
{
public:
	APieModel(const PieCaps& def_caps);
	virtual ~APieModel();

	virtual unsigned version() const =0;

	virtual bool read(std::istream& in);
	virtual void write(std::ostream& out, const PieCaps* piecaps = nullptr) const;

	size_t levels() const;
	virtual unsigned getType() const;

	bool isValid() const;

	const PieCaps& getCaps() const {return m_caps;}

	virtual bool isFeatureSet(unsigned feature) const;
protected:

	void clearAll();

	virtual unsigned textureHeight() const =0;
	virtual unsigned textureWidth() const =0;

	virtual bool readHeaderBlock(std::istream& in);

	virtual bool readTexturesBlock(std::istream& in);
	bool readTextureDirective(std::istream& in);
	bool readNormalmapDirective(std::istream& in);
	bool readSpecmapDirective(std::istream& in);

	virtual bool readLevelsBlock(std::istream& in);
	bool readEventsDirective(std::istream& in);
	int readLevelsDirective(std::istream& in);
	bool readLevels(int levels, std::istream& in);
	bool readAnimObjectDirective(std::istream &in);

	std::string m_texture;
	std::string m_texture_normalmap;
	std::string m_texture_tcmask;
	std::string m_texture_specmap;
	std::map<int, std::string> m_events; // Animation events associated with this model

	std::vector<L> m_levels;

	unsigned int m_read_type;
	const PieCaps m_def_caps;
	PieCaps m_caps;
};

template <typename V>
struct PieConnector
{
	virtual ~PieConnector(){}
	bool read(std::istream& in);
	void write(std::ostream& out) const;
	V pos;
};


/** Returns the Pie version
  *
  *	@param	in	istream to a Pie file, the stream's position will be
  *		returned to where it was (on success).
  *	@return	int Version of the pie version.
  */
int pieVersion(std::istream& in);

/**********************************************
  Pie version 2
  *********************************************/
typedef UV<GLushort> Pie2UV;
typedef Vertex<GLint> Pie2Vertex;
typedef PieConnector<Pie2Vertex> Pie2Connector;

class Pie2Polygon : public PiePolygon<Pie2UV, GLushort, 16>
{
	friend class Pie3Polygon; // only for operator thisclass() and thatclass(const thisclass&)
public:
	Pie2Polygon(){}
	virtual ~Pie2Polygon(){}
};

class Pie2Level : public APieLevel<Pie2Vertex, Pie2Polygon, Pie2Connector>
{
	friend class Pie3Level; // only for operator thisclass() and thatclass(const thisclass&)
public:
	Pie2Level(){}
	virtual ~Pie2Level(){}
};

class Pie2Model : public APieModel<Pie2Level>
{
	friend class Pie3Model; // only for operator thisclass() and thatclass(const thisclass&)
public:
	Pie2Model();
	virtual ~Pie2Model();

	unsigned version() const;

	unsigned textureHeight() const;
	unsigned textureWidth() const;
};

/**********************************************
  Pie version 3
  *********************************************/
class Pie3UV : public UV<GLclampf>
{
public:
	Pie3UV();
	Pie3UV(GLclampf u, GLclampf v);
	Pie3UV(const Pie2UV& p2);
	operator Pie2UV() const;
};

class Pie3Vertex : public Vertex<GLfloat>
{
public:
	Pie3Vertex(){}
	Pie3Vertex(const Vertex<GLfloat>& vert);
	Pie3Vertex(const Pie2Vertex& p2);

	static inline Pie3Vertex upConvert(const Pie2Vertex& p2);
	static Pie2Vertex backConvert(const Pie3Vertex& p3);
	operator Pie2Vertex() const;
};

class Pie3Connector : public PieConnector<Pie3Vertex>
{
public:
	Pie3Connector(){}
	Pie3Connector(const Pie2Connector& p2);

	static Pie3Connector upConvert(const Pie2Connector& p2);
	static Pie2Connector backConvert(const Pie3Connector& p3);
	operator Pie2Connector() const;
};

class Pie3Polygon : public PiePolygon<Pie3UV, GLclampf, 3>
{
public:
	Pie3Polygon();
	virtual ~Pie3Polygon();

	static int upConvert(const Pie2Polygon& pie2Poly, std::back_insert_iterator<std::vector<Pie3Polygon> > result);
	static Pie2Polygon backConvert(const Pie3Polygon& p3);
	operator Pie2Polygon() const;

	Pie3UV getUV(unsigned index, unsigned frame) const;

private:
};

class Pie3Level : public APieLevel<Pie3Vertex, Pie3Polygon, Pie3Connector>
{
    friend WZM::WZM(const Pie3Model &p3);
    friend WZM::operator Pie3Model() const;
public:
	Pie3Level();
	Pie3Level(const Pie2Level& p2);
	virtual ~Pie3Level();

	static Pie3Level upConvert(const Pie2Level& p2);
	static Pie2Level backConvert(const Pie3Level& p3);
	operator Pie2Level() const;
};

class Pie3Model : public APieModel<Pie3Level>
{
	friend WZM::WZM(const Pie3Model &p3);
	friend WZM::operator Pie3Model() const;
public:
	Pie3Model();
	Pie3Model(const Pie2Model& pie2);
	virtual ~Pie3Model();

	unsigned version() const;

	operator Pie2Model() const;

	bool setType(int type);

protected:
	unsigned textureHeight() const;
	unsigned textureWidth() const;
};

// Include template implementations
#include "Pie_t.hpp"

#endif // PIE_HPP
