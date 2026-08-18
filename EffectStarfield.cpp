#include "EffectStarfield.h"



#include <cmath>
#include "vipgfx.h"

namespace
{
	const float PI = 3.1415926f;

	float clamp01(float value)
	{
		if (value < 0.0f) return 0.0f;
		if (value > 1.0f) return 1.0f;
		return value;
	}

	float smoothStep(float value)
	{
		value = clamp01(value);
		return value * value * (3.0f - 2.0f * value);
	}

	float smoothTurn(float value)
	{
		value = clamp01(value);
		return value * value * value
			* (value * (value * 6.0f - 15.0f) + 10.0f);
	}

	float starfieldRotation(float time)
	{
		const float start = 2.0f;
		const float turnDuration = 4.0f;
		const float fullTurn = PI * 2.0f;

		if (time < start)
			return 0.0f;
		if (time < start + turnDuration)
			return fullTurn
				* smoothTurn((time - start) / turnDuration);
		if (time < start + turnDuration * 2.0f)
			return fullTurn
				- fullTurn * smoothTurn(
					(time - start - turnDuration) / turnDuration
				);
		if (time < start + turnDuration * 3.0f)
			return fullTurn
				* smoothTurn(
					(time - start - turnDuration * 2.0f) / turnDuration
				);
		return fullTurn;
	}

	float randomValue(unsigned int index, unsigned int channel)
	{
		unsigned int value =
			index * 747796405u + channel * 2891336453u + 277803737u;
		value ^= value >> 16;
		value *= 2246822519u;
		value ^= value >> 13;
		return (float)(value & 65535u) / 65535.0f;
	}

	float wrappedDepth(float base, float travel)
	{
		float phase = base - travel;
		phase -= floorf(phase);
		if (phase < 0.0f)
			phase += 1.0f;
		return 0.10f + phase * 0.90f;
	}

	void starColor(
		unsigned int index,
		float depth,
		float time,
		float alpha
	)
	{
		float phase =
			randomValue(index, 3u) * PI * 2.0f
			+ time * 0.22f
			+ depth * 1.5f;
		float red = 0.55f + 0.45f * sinf(phase);
		float green = 0.65f + 0.35f * sinf(phase + 2.0944f);
		float blue = 0.75f + 0.25f * sinf(phase + 4.1888f);

		glColor4f(red, green, blue, alpha);
	}

	void projectStar(
		unsigned int index,
		float depth,
		float spiralStrength,
		float rotationAngle,
		float gather,
		float aspect,
		float* x,
		float* y
	)
	{
		float baseX = (randomValue(index, 0u) * 2.0f - 1.0f) * 0.92f;
		float baseY = (randomValue(index, 1u) * 2.0f - 1.0f) * 0.92f;
		float angle =
			rotationAngle
			+ spiralStrength * (1.0f - depth) * 1.8f;
		float ca = cosf(angle);
		float sa = sinf(angle);
		float rotatedX = baseX * ca - baseY * sa;
		float rotatedY = baseX * sa + baseY * ca;
		float perspective = 0.64f / depth;
		float gatherScale = 1.0f - gather;

		*x = rotatedX * perspective * aspect * gatherScale;
		*y = rotatedY * perspective * gatherScale;
	}

	// Multiples of 8x16 keep every checker edge on a mesh boundary.
	const int kMorphLatitudeBands = 40;
	const int kMorphLongitudeSegments = 64;

	struct MorphVertex
	{
		float x, y, z;
		float nx, ny, nz;
		float flow;
		float morphAmount;
		float alpha;
		float whiteRed, whiteGreen, whiteBlue;
		float patchRed, patchGreen, patchBlue;
	};

	MorphVertex gMorphMesh
		[kMorphLatitudeBands + 1][kMorphLongitudeSegments + 1];

	struct MorphSource
	{
		float x, y, z;
		float noise[3][3];
	};

	MorphSource gMorphSources
		[kMorphLatitudeBands + 1][kMorphLongitudeSegments + 1];
	bool gMorphSourcesReady = false;

	void normalizeDirection(float* x, float* y, float* z)
	{
		float length = sqrtf(*x * *x + *y * *y + *z * *z);
		if (length <= 0.000001f)
			return;
		*x /= length;
		*y /= length;
		*z /= length;
	}

	float morphCap(
		float x,
		float y,
		float z,
		float directionX,
		float directionY,
		float directionZ,
		float edge
	)
	{
		float dot =
			x * directionX + y * directionY + z * directionZ;
		return smoothStep((dot - edge) / (1.0f - edge));
	}

	float mixValue(float first, float second, float amount)
	{
		return first + (second - first) * amount;
	}

	float noiseHash(int x, int y, int z, unsigned int seed)
	{
		unsigned int value =
			(unsigned int)x * 1619u
			+ (unsigned int)y * 31337u
			+ (unsigned int)z * 6971u
			+ seed * 1013u;
		value ^= value >> 13;
		value *= 60493u;
		value ^= value >> 17;
		return (float)(value & 65535u) / 32767.5f - 1.0f;
	}

	float valueNoise3D(
		float x,
		float y,
		float z,
		unsigned int seed
	)
	{
		int x0 = (int)floorf(x);
		int y0 = (int)floorf(y);
		int z0 = (int)floorf(z);
		float tx = smoothStep(x - (float)x0);
		float ty = smoothStep(y - (float)y0);
		float tz = smoothStep(z - (float)z0);
		float x00 = mixValue(
			noiseHash(x0, y0, z0, seed),
			noiseHash(x0 + 1, y0, z0, seed),
			tx
		);
		float x10 = mixValue(
			noiseHash(x0, y0 + 1, z0, seed),
			noiseHash(x0 + 1, y0 + 1, z0, seed),
			tx
		);
		float x01 = mixValue(
			noiseHash(x0, y0, z0 + 1, seed),
			noiseHash(x0 + 1, y0, z0 + 1, seed),
			tx
		);
		float x11 = mixValue(
			noiseHash(x0, y0 + 1, z0 + 1, seed),
			noiseHash(x0 + 1, y0 + 1, z0 + 1, seed),
			tx
		);
		return mixValue(
			mixValue(x00, x10, ty),
			mixValue(x01, x11, ty),
			tz
		);
	}

	void initializeMorphSources()
	{
		if (gMorphSourcesReady)
			return;

		const float scales[3] = { 1.65f, 3.35f, 6.20f };
		for (int latitudeBand = 0;
			latitudeBand <= kMorphLatitudeBands;
			latitudeBand++)
		{
			float latitude =
				-PI * 0.5f
				+ PI * (float)latitudeBand / (float)kMorphLatitudeBands;
			float cosLatitude = cosf(latitude);
			float sinLatitude = sinf(latitude);

			for (int longitudeSegment = 0;
				longitudeSegment <= kMorphLongitudeSegments;
				longitudeSegment++)
			{
				float longitude =
					PI * 2.0f * (float)longitudeSegment
					/ (float)kMorphLongitudeSegments;
				MorphSource* source =
					&gMorphSources[latitudeBand][longitudeSegment];
				source->x = cosLatitude * cosf(longitude);
				source->y = sinLatitude;
				source->z = cosLatitude * sinf(longitude);

				for (int octave = 0; octave < 3; octave++)
				{
					unsigned int channelOffset =
						(unsigned int)octave * 2246822519u;
					float x = source->x * scales[octave];
					float y = source->y * scales[octave];
					float z = source->z * scales[octave];
					source->noise[octave][0] = valueNoise3D(
						x, y, z, 1013904223u + channelOffset
					);
					source->noise[octave][1] = valueNoise3D(
						x, y, z, 2654435761u + channelOffset
					);
					source->noise[octave][2] = valueNoise3D(
						x, y, z, 2246822519u + channelOffset
					);
				}
			}
		}

		gMorphSourcesReady = true;
	}

	float morphNoise3D(
		const MorphSource* source,
		int octave,
		float firstWeight,
		float secondWeight,
		float thirdWeight
	)
	{
		return (
			source->noise[octave][0] * firstWeight
			+ source->noise[octave][1] * secondWeight
			+ source->noise[octave][2] * thirdWeight
		) * 0.69f;
	}

	void buildMorphMesh(
		float time,
		float radius,
		float horizontalScale,
		float rotationAngle,
		float surfaceMorph
	)
	{
		int latitudeBand;
		int longitudeSegment;
		initializeMorphSources();
		time *= 1.65f;
		float cosRotation = cosf(rotationAngle);
		float sinRotation = sinf(rotationAngle);
		float meteorVerticalScale =
			1.0f + 0.11f * sinf(time * 1.15f + 1.20f);
		float verticalScale =
			mixValue(1.0f, meteorVerticalScale, surfaceMorph);
		float shearX =
			0.13f * sinf(time * 0.90f + 0.30f) * surfaceMorph;
		float shearY =
			0.11f * sinf(time * 1.10f + 1.40f) * surfaceMorph;
		float direction1X = 0.78f + 0.15f * sinf(time * 0.43f);
		float direction1Y = 0.47f + 0.13f * sinf(time * 0.61f + 1.0f);
		float direction1Z = 0.41f + 0.12f * sinf(time * 0.37f + 2.0f);
		float direction2X = -0.72f + 0.14f * sinf(time * 0.51f + 0.6f);
		float direction2Y = 0.56f + 0.12f * sinf(time * 0.39f + 2.1f);
		float direction2Z = 0.40f + 0.13f * sinf(time * 0.67f + 1.4f);
		float direction3X = 0.18f + 0.16f * sinf(time * 0.59f + 2.5f);
		float direction3Y = -0.92f + 0.10f * sinf(time * 0.41f + 0.3f);
		float direction3Z = 0.34f + 0.15f * sinf(time * 0.73f + 1.2f);
		float direction4X = -0.52f + 0.15f * sinf(time * 0.47f + 1.8f);
		float direction4Y = -0.68f + 0.14f * sinf(time * 0.63f + 0.9f);
		float direction4Z = 0.52f + 0.12f * sinf(time * 0.35f + 2.7f);
		float direction5X = 0.62f + 0.17f * sinf(time * 0.57f + 1.1f);
		float direction5Y = 0.12f + 0.18f * sinf(time * 0.45f + 2.4f);
		float direction5Z = -0.77f + 0.11f * sinf(time * 0.69f + 0.2f);
		float direction6X = -0.18f + 0.16f * sinf(time * 0.65f + 2.0f);
		float direction6Y = 0.84f + 0.12f * sinf(time * 0.33f + 0.7f);
		float direction6Z = -0.51f + 0.14f * sinf(time * 0.55f + 1.5f);
		float strength1 = 0.36f + 0.18f * sinf(time * 0.73f);
		float strength2 = 0.40f + 0.17f * sinf(time * 0.91f + 1.2f);
		float strength3 = 0.32f + 0.18f * sinf(time * 0.57f + 2.1f);
		float strength4 = 0.36f + 0.17f * sinf(time * 0.83f + 0.4f);
		float strength5 = 0.38f * sinf(time * 0.69f + 1.7f);
		float strength6 = 0.36f * sinf(time * 0.81f + 2.6f);

		normalizeDirection(&direction1X, &direction1Y, &direction1Z);
		normalizeDirection(&direction2X, &direction2Y, &direction2Z);
		normalizeDirection(&direction3X, &direction3Y, &direction3Z);
		normalizeDirection(&direction4X, &direction4Y, &direction4Z);
		normalizeDirection(&direction5X, &direction5Y, &direction5Z);
		normalizeDirection(&direction6X, &direction6Y, &direction6Z);
		float noiseAngle = time * 0.74f;
		float firstNoiseWeight = cosf(noiseAngle);
		float secondNoiseWeight = sinf(noiseAngle);
		float thirdNoiseWeight =
			0.45f * sinf(time * 1.13f + 0.80f);

		for (latitudeBand = 0;
			latitudeBand <= kMorphLatitudeBands;
			latitudeBand++)
		{
			for (longitudeSegment = 0;
				longitudeSegment <= kMorphLongitudeSegments;
				longitudeSegment++)
			{
				const MorphSource* source =
					&gMorphSources[latitudeBand][longitudeSegment];
				float ux = source->x;
				float uy = source->y;
				float uz = source->z;
				float largeMorph =
					strength1 * morphCap(
						ux, uy, uz,
						direction1X, direction1Y, direction1Z, 0.02f
					)
					- strength2 * morphCap(
						ux, uy, uz,
						direction2X, direction2Y, direction2Z, 0.20f
					)
					+ strength3 * morphCap(
						ux, uy, uz,
						direction3X, direction3Y, direction3Z, 0.08f
					)
					- strength4 * morphCap(
						ux, uy, uz,
						direction4X, direction4Y, direction4Z, 0.24f
					)
					+ strength5 * morphCap(
						ux, uy, uz,
						direction5X, direction5Y, direction5Z, 0.14f
					)
					+ strength6 * morphCap(
						ux, uy, uz,
						direction6X, direction6Y, direction6Z, 0.18f
					);
				float largeNoise = morphNoise3D(
					source, 0,
					firstNoiseWeight, secondNoiseWeight, thirdNoiseWeight
				);
				float mediumNoise = morphNoise3D(
					source, 1,
					firstNoiseWeight, secondNoiseWeight, thirdNoiseWeight
				);
				float fineNoise = morphNoise3D(
					source, 2,
					firstNoiseWeight, secondNoiseWeight, thirdNoiseWeight
				);
				float furrow =
					1.0f / (1.0f + 26.0f * fineNoise * fineNoise);
				float deformation =
					largeMorph * 0.52f
					+ largeNoise * 0.22f
					+ mediumNoise * 0.15f
					+ fineNoise * 0.075f
					- (furrow - 0.18f) * 0.14f;

				if (deformation < -0.54f) deformation = -0.54f;
				if (deformation > 0.50f) deformation = 0.50f;
				deformation *= surfaceMorph;

				float localRadius = radius * (1.0f + deformation);
				MorphVertex* vertex =
					&gMorphMesh[latitudeBand][longitudeSegment];
				float rawX = ux * localRadius;
				float rawY = uy * localRadius * verticalScale;

				vertex->x =
					(rawX * cosRotation - rawY * sinRotation)
					* horizontalScale
					+ uz * localRadius * shearX * horizontalScale;
				vertex->y =
					rawX * sinRotation + rawY * cosRotation
					+ uz * localRadius * shearY;
				vertex->z = uz * localRadius;
				vertex->flow = clamp01(
					0.50f
					+ largeNoise * 0.24f
					+ mediumNoise * 0.32f
					- furrow * 0.18f
				);
				vertex->morphAmount = clamp01(
					(fabsf(deformation) * 2.4f + furrow * 0.45f)
					* surfaceMorph
				);
				vertex->alpha = clamp01(
					0.60f + vertex->flow * 0.24f
					- vertex->morphAmount * 0.055f
				);
			}
		}

		// Derive normals from the deformed mesh, not the source sphere.
		for (latitudeBand = 0;
			latitudeBand <= kMorphLatitudeBands;
			latitudeBand++)
		{
			int previousLatitude = latitudeBand > 0
				? latitudeBand - 1 : latitudeBand;
			int nextLatitude = latitudeBand < kMorphLatitudeBands
				? latitudeBand + 1 : latitudeBand;

			for (longitudeSegment = 0;
				longitudeSegment <= kMorphLongitudeSegments;
				longitudeSegment++)
			{
				int previousLongitude = longitudeSegment > 0
					? longitudeSegment - 1 : kMorphLongitudeSegments - 1;
				int nextLongitude = longitudeSegment < kMorphLongitudeSegments
					? longitudeSegment + 1 : 1;
				MorphVertex* vertex =
					&gMorphMesh[latitudeBand][longitudeSegment];
				const MorphVertex* latitude0 =
					&gMorphMesh[previousLatitude][longitudeSegment];
				const MorphVertex* latitude1 =
					&gMorphMesh[nextLatitude][longitudeSegment];
				const MorphVertex* longitude0 =
					&gMorphMesh[latitudeBand][previousLongitude];
				const MorphVertex* longitude1 =
					&gMorphMesh[latitudeBand][nextLongitude];
				float latX = latitude1->x - latitude0->x;
				float latY = latitude1->y - latitude0->y;
				float latZ = latitude1->z - latitude0->z;
				float lonX = longitude1->x - longitude0->x;
				float lonY = longitude1->y - longitude0->y;
				float lonZ = longitude1->z - longitude0->z;
				float nx = latY * lonZ - latZ * lonY;
				float ny = latZ * lonX - latX * lonZ;
				float nz = latX * lonY - latY * lonX;
				float length = sqrtf(nx * nx + ny * ny + nz * nz);

				if (length > 0.000001f)
				{
					vertex->nx = nx / length;
					vertex->ny = ny / length;
					vertex->nz = nz / length;
				}
				else
				{
					float fallbackLength = sqrtf(
						vertex->x * vertex->x
						+ vertex->y * vertex->y
						+ vertex->z * vertex->z
					);
					vertex->nx = vertex->x / fallbackLength;
					vertex->ny = vertex->y / fallbackLength;
					vertex->nz = vertex->z / fallbackLength;
				}
			}
		}
	}

	void buildMorphLighting(
		float surfaceMorph,
		const GLfloat* modelView)
	{
		for (int latitudeBand = 0;
			latitudeBand <= kMorphLatitudeBands;
			latitudeBand++)
		{
			for (int longitudeSegment = 0;
				longitudeSegment <= kMorphLongitudeSegments;
				longitudeSegment++)
			{
				MorphVertex* vertex =
					&gMorphMesh[latitudeBand][longitudeSegment];
				// Transform normals without translation so directional light stays
				// in view space. Do this once per mesh point, not once per triangle.
				float normalX =
					modelView[0] * vertex->nx
					+ modelView[4] * vertex->ny
					+ modelView[8] * vertex->nz;
				float normalY =
					modelView[1] * vertex->nx
					+ modelView[5] * vertex->ny
					+ modelView[9] * vertex->nz;
				float normalZ =
					modelView[2] * vertex->nx
					+ modelView[6] * vertex->ny
					+ modelView[10] * vertex->nz;
				float normalLength = sqrtf(
					normalX * normalX
					+ normalY * normalY
					+ normalZ * normalZ
				);

				if (normalLength > 0.000001f)
				{
					normalX /= normalLength;
					normalY /= normalLength;
					normalZ /= normalLength;
				}

				float diffuse = clamp01(
					-normalX * 0.30f
					+ normalY * 0.48f
					+ normalZ * 0.82f
				);
				float light = 0.18f + diffuse * 0.82f;
				float sheen = clamp01(
					-normalX * 0.34f
					+ normalY * 0.54f
					+ normalZ * 0.77f
				);
				float rim = clamp01(1.0f - fabsf(normalZ));
				float shapeShade =
					1.0f
					- vertex->morphAmount * (1.0f - diffuse) * 0.24f;

				sheen *= sheen;
				sheen *= sheen;
				sheen *= sheen;
				rim *= rim;

				float meteorRed = clamp01(
					(0.025f + (1.0f - vertex->flow) * 0.075f)
					* light * shapeShade + sheen * 0.30f + rim * 0.015f
				);
				float meteorGreen = clamp01(
					(0.20f + vertex->flow * 0.52f)
					* light * shapeShade + sheen * 0.48f + rim * 0.075f
				);
				float meteorBlue = clamp01(
					(0.065f + (1.0f - vertex->flow) * 0.23f)
					* light * shapeShade + sheen * 0.28f + rim * 0.11f
				);
				float checkerLight = 0.30f + diffuse * 0.70f;
				float checkerSheen = sheen * 0.18f;

				vertex->whiteRed = mixValue(
					clamp01(0.98f * checkerLight + checkerSheen),
					meteorRed,
					surfaceMorph
				);
				vertex->whiteGreen = mixValue(
					clamp01(0.98f * checkerLight + checkerSheen),
					meteorGreen,
					surfaceMorph
				);
				vertex->whiteBlue = mixValue(
					clamp01(0.96f * checkerLight + checkerSheen),
					meteorBlue,
					surfaceMorph
				);
				vertex->patchRed = mixValue(
					clamp01(0.92f * checkerLight + checkerSheen),
					meteorRed,
					surfaceMorph
				);
				vertex->patchGreen = mixValue(
					clamp01(0.025f * checkerLight + checkerSheen),
					meteorGreen,
					surfaceMorph
				);
				vertex->patchBlue = mixValue(
					clamp01(0.035f * checkerLight + checkerSheen),
					meteorBlue,
					surfaceMorph
				);
			}
		}
	}

	void drawMorphVertex(
		int latitudeBand,
		int longitudeSegment,
		bool redPatch)
	{
		const MorphVertex* vertex =
			&gMorphMesh[latitudeBand][longitudeSegment];
		if (redPatch)
			glColor4f(
				vertex->patchRed,
				vertex->patchGreen,
				vertex->patchBlue,
				1.0f
			);
		else
			glColor4f(
				vertex->whiteRed,
				vertex->whiteGreen,
				vertex->whiteBlue,
				1.0f
			);
		glVertex3f(vertex->x, vertex->y, vertex->z);
	}

	void drawMorphGlowShell(float scale, float alpha)
	{
		glPushMatrix();
		glScalef(scale, scale, scale);
		glColor4f(0.16f, 1.0f, 0.30f, alpha);

		for (int latitudeBand = 0;
			latitudeBand < kMorphLatitudeBands;
			latitudeBand++)
		{
			glBegin(GL_TRIANGLE_STRIP);
			for (int longitudeSegment = 0;
				longitudeSegment <= kMorphLongitudeSegments;
				longitudeSegment++)
			{
				const MorphVertex* first =
					&gMorphMesh[latitudeBand][longitudeSegment];
				const MorphVertex* second =
					&gMorphMesh[latitudeBand + 1][longitudeSegment];
				glVertex3f(first->x, first->y, first->z);
				glVertex3f(second->x, second->y, second->z);
			}
			glEnd();
		}

		glPopMatrix();
	}

	void drawMeteorMorphMesh(
		float time,
		float radius,
		float rotationAngle,
		float surfaceMorph
	)
	{
		if (radius <= 0.0001f)
			return;

		float meteorHorizontalScale =
			0.76f * (1.0f + 0.075f * sinf(time * 1.55f + 0.40f));
		float horizontalScale =
			mixValue(1.0f, meteorHorizontalScale, surfaceMorph);

		buildMorphMesh(
			time,
			radius,
			horizontalScale,
			rotationAngle,
			surfaceMorph
		);

		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		GLfloat modelView[16];
		glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
		buildMorphLighting(surfaceMorph, modelView);

		// Per-triangle field color preserves hard checker edges with smooth lighting.
		glShadeModel(GL_SMOOTH);
		glBegin(GL_TRIANGLES);
		for (int latitudeBand = 0;
			latitudeBand < kMorphLatitudeBands;
			latitudeBand++)
		{
			int checkerLatitude =
				latitudeBand * 8 / kMorphLatitudeBands;

			for (int longitudeSegment = 0;
				longitudeSegment < kMorphLongitudeSegments;
				longitudeSegment++)
			{
				int checkerLongitude =
					longitudeSegment * 16 / kMorphLongitudeSegments;
				bool redPatch =
					((checkerLatitude + checkerLongitude) & 1) != 0;

				drawMorphVertex(
					latitudeBand, longitudeSegment, redPatch
				);
				drawMorphVertex(
					latitudeBand + 1, longitudeSegment, redPatch
				);
				drawMorphVertex(
					latitudeBand + 1, longitudeSegment + 1, redPatch
				);

				drawMorphVertex(
					latitudeBand, longitudeSegment, redPatch
				);
				drawMorphVertex(
					latitudeBand + 1, longitudeSegment + 1, redPatch
				);
				drawMorphVertex(
					latitudeBand, longitudeSegment + 1, redPatch
				);
			}
		}
		glEnd();

		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
		glCullFace(GL_FRONT);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		drawMorphGlowShell(1.075f, 0.12f * surfaceMorph);
		drawMorphGlowShell(1.040f, 0.36f * surfaceMorph);
		glDepthMask(GL_TRUE);
		glCullFace(GL_BACK);

		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}

	void drawLiquidSphere(
		float time,
		float gather,
		float rotationAngle)
	{
		float collapseScale = 1.0f - gather;
		if (collapseScale <= 0.003f)
			return;

		float pulse =
			1.0f
			+ 0.14f * sinf(time * 1.55f)
			+ 0.050f * sinf(time * 3.10f + 0.70f);
		float radius = 0.205f * pulse * collapseScale;
		drawMeteorMorphMesh(time, radius, rotationAngle, 1.0f);
	}

}

void prepareStarfieldEffect()
{
	initializeMorphSources();
}

void drawStarfieldMeteorMorph(
	float time,
	float radius,
	float morphAmount,
	float rotationAngle)
{
	drawMeteorMorphMesh(
		time,
		radius,
		rotationAngle,
		clamp01(morphAmount)
	);
}

void drawStarfieldEffect(float animTime, float duration)
{
	int w = vscreen.width;
	int h = vscreen.height;
	float aspect = (h > 0) ? (float)w / (float)h : 1.3333f;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-aspect, aspect, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClearColor(0.002f, 0.004f, 0.015f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glShadeModel(GL_SMOOTH);

	float spiral = smoothStep((animTime - 2.0f) / 1.2f);
	float rotationAngle = starfieldRotation(animTime);
	float previousRotationAngle = starfieldRotation(animTime - 0.045f);
	float previousSpiral =
		smoothStep((animTime - 0.045f - 2.0f) / 1.2f);
	float gatherStart = duration - 4.0f;
	float gather = smoothStep((animTime - gatherStart) / 4.0f);
	float fade = smoothTurn(animTime / 1.10f);

	const int starCount = 220;
	const float speed = 0.27f;

	glLineWidth(4.5f);
	glBegin(GL_LINES);
	for (int i = 0; i < starCount; i++)
	{
		unsigned int index = (unsigned int)i;
		float baseDepth = randomValue(index, 2u);
		float depth = wrappedDepth(baseDepth, animTime * speed);
		float previousDepth =
			wrappedDepth(baseDepth, animTime * speed - 0.012f);
		float x, y, previousX, previousY;

		// Suppress a trail across the depth wrap.
		if (previousDepth < depth)
			previousDepth = depth;

		projectStar(
			index,
			previousDepth,
			previousSpiral,
			previousRotationAngle,
			gather,
			aspect,
			&previousX,
			&previousY
		);
		projectStar(
			index,
			depth,
			spiral,
			rotationAngle,
			gather,
			aspect,
			&x,
			&y
		);

		float brightness = clamp01((1.05f - depth) * 1.15f) * fade;
		starColor(index, depth, animTime, brightness * 0.10f);
		glVertex2f(previousX, previousY);
		starColor(index, depth, animTime, brightness * 0.72f);
		glVertex2f(x, y);
	}
	glEnd();

	glPointSize(3.5f);
	glBegin(GL_POINTS);
	for (int i = 0; i < starCount; i++)
	{
		unsigned int index = (unsigned int)i;
		float depth =
			wrappedDepth(randomValue(index, 2u), animTime * speed);
		float x, y;
		projectStar(
			index,
			depth,
			spiral,
			rotationAngle,
			gather,
			aspect,
			&x,
			&y
		);

		float twinkle =
			0.72f + 0.28f
			* sinf(animTime * 5.0f + randomValue(index, 4u) * 20.0f);
		float brightness =
			clamp01((1.08f - depth) * 1.25f) * twinkle * fade;
		starColor(index, depth, animTime, brightness);
		glVertex2f(x, y);
	}
	glEnd();

	drawLiquidSphere(animTime, gather, -rotationAngle);

	glPointSize(1.0f);
	glLineWidth(1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}
