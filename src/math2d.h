#pragma once

#ifndef __MATH_2D__
#define __MATH_2D__

typedef struct D2D_MATRIX_3X3_F
{
	FLOAT _11;
	FLOAT _12;
	FLOAT _21;
	FLOAT _22;
	FLOAT _31;
	FLOAT _32;
	//
	FLOAT _13;
	FLOAT _23;
	FLOAT _33;
} D2D_MATRIX_3X3_F;

namespace D2D1
{
	inline Matrix3x2F Invert(const Matrix3x2F& m) {
		D2D_MATRIX_3X2_F r;

		float det =
			m._11 * (m._22) -
			m._12 * (m._21);

		float invdet = 1 / det;

		r._11 = invdet * m._22;
		r._12 = invdet * (-m._12);
	
		r._21 = invdet * (-m._21);
		r._22 = invdet * m._11;
	
		r._31 = invdet * (m._21 * m._32 - m._22 * m._31);
		r._32 = invdet * (m._12 * m._31 - m._11 * m._32);
		
		return reinterpret_cast<Matrix3x2F&>(r);
	}

	class MyPoint2F : public D2D1_POINT_2F
	{
		friend MyPoint2F operator+(const MyPoint2F& p1, const MyPoint2F& p2);
		friend MyPoint2F operator-(const MyPoint2F& p1, const MyPoint2F& p2);
	public:
		MyPoint2F();
		MyPoint2F(float x, float y);
		MyPoint2F(const D2D1_POINT_2F& pt);

	//	void operator=(const MyPoint2F& p);
		
		void operator=(const D2D1_POINT_2F& p);

		MyPoint2F& operator=(const MyPoint2F& p);
		bool operator==(const MyPoint2F& p) const;

		MyPoint2F operator*(float scalar) noexcept {
			return MyPoint2F{x * scalar, y * scalar};
		}

		operator CPoint();
	};
	inline MyPoint2F::MyPoint2F() {

	}
	inline MyPoint2F::MyPoint2F(float x, float y) {
		this->x = x;
		this->y = y;
	}
	inline MyPoint2F::MyPoint2F(const D2D1_POINT_2F& pt) {
		this->x = pt.x;
		this->y = pt.y;
	}
	inline MyPoint2F& MyPoint2F::operator=(const MyPoint2F& p) {
		this->x = p.x;
		this->y = p.y;
		return *this;
	}
	inline void MyPoint2F::operator=(const D2D1_POINT_2F& p) {
		this->x = p.x;
		this->y = p.y;
	}
	inline bool MyPoint2F::operator==(const MyPoint2F& p) const {
		return x == p.x && y == p.y;
	}
	inline MyPoint2F operator+(const MyPoint2F& p1, const MyPoint2F& p2) {
		return MyPoint2F(p1.x + p2.x, p1.y + p2.y);
	}
	inline MyPoint2F operator-(const MyPoint2F& p1, const MyPoint2F& p2) {
		return MyPoint2F(p1.x - p2.x, p1.y - p2.y);
	}
	inline MyPoint2F::operator CPoint() { 
		return CPoint((LONG)x, (LONG)y); 
	}
}

/*
float det =
t._11 * (t._22 * t._33 - t._23 * t._32) -
t._12 * (t._21 * t._33 - t._23 * t._31) +
t._13 * (t._21 * t._32 - t._22 * t._31);

float invdet = 1 / det;

res._11 = invdet * (t._22 * t._33 - t._23 * t._32);
res._12 = invdet * (t._13 * t._32 - t._12 * t._33);
res._13 = invdet * (t._12 * t._23 - t._13 * t._22);
res._21 = invdet * (t._23 * t._31 - t._21 * t._33);
res._22 = invdet * (t._11 * t._33 - t._13 * t._31);
res._23 = invdet * (t._13 * t._21 - t._11 * t._21);
res._31 = invdet * (t._21 * t._32 - t._22 * t._31);
res._32 = invdet * (t._12 * t._31 - t._11 * t._32);
res._33 = invdet * (t._11 * t._22 - t._12 * t._21);
*/


#endif // __MATH_2D__