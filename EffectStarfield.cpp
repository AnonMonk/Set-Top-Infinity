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

	const int kMorphLatitudeBands = 34;
	const int kMorphLongitudeSegments = 52;

	struct MorphVertex
	{
		float x, y, z;
		float nx, ny, nz;
		float flow;
		float morphAmount;
		float alpha;
	};

	MorphVertex gMorphMesh
		[kMorphLatitudeBands + 1][kMorphLongitudeSegments + 1];

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

	float morphNoise3D(
		float x,
		float y,
		float z,
		float firstWeight,
		float secondWeight,
		float thirdWeight,
		unsigned int channel
	)
	{
		unsigned int channelOffset = channel * 2246822519u;
		float first = valueNoise3D(
			x, y, z, 1013904223u + channelOffset
		);
		float second = valueNoise3D(
			x, y, z, 2654435761u + channelOffset
		);
		float third = valueNoise3D(
			x, y, z, 2246822519u + channelOffset
		);
		return (
			first * firstWeight
			+ second * secondWeight
			+ third * thirdWeight
		) * 0.69f;
	}

	void buildMorphMesh(
		float time,
		float radius,
		float horizontalScale,
		float rotationAngle
	)
	{
		int latitudeBand;
		int longitudeSegment;
		/* Interne Formbewegung bewusst schneller als die Demo-Uhr. */
		time *= 1.65f;
		float cosRotation = cosf(rotationAngle);
		float sinRotation = sinf(rotationAngle);
		float verticalScale =
			1.0f + 0.11f * sinf(time * 1.15f + 1.20f);
		float shearX = 0.13f * sinf(time * 0.90f + 0.30f);
		float shearY = 0.11f * sinf(time * 1.10f + 1.40f);
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
		/* Sin/Cos-Paar: konstante Morphbewegung ohne Ruhepunkt. */
		float noiseAngle = time * 0.74f;
		float firstNoiseWeight = cosf(noiseAngle);
		float secondNoiseWeight = sinf(noiseAngle);
		float thirdNoiseWeight =
			0.45f * sinf(time * 1.13f + 0.80f);

		/* Wandernde lokale Morphzentren statt radialer Oberflaechenmuster. */
		for (latitudeBand = 0;
			latitudeBand <= kMorphLatitudeBands;
			latitudeBand++)
		{
			float latitude =
				-PI * 0.5f
				+ PI * (float)latitudeBand / (float)kMorphLatitudeBands;
			float cosLatitude = cosf(latitude);
			float sinLatitude = sinf(latitude);

			for (longitudeSegment = 0;
				longitudeSegment <= kMorphLongitudeSegments;
				longitudeSegment++)
			{
				float longitude =
					PI * 2.0f * (float)longitudeSegment
					/ (float)kMorphLongitudeSegments;
				float cosLongitude = cosf(longitude);
				float sinLongitude = sinf(longitude);
				float ux = cosLatitude * cosLongitude;
				float uy = sinLatitude;
				float uz = cosLatitude * sinLongitude;
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
					ux * 1.65f, uy * 1.65f, uz * 1.65f,
					firstNoiseWeight, secondNoiseWeight, thirdNoiseWeight, 0u
				);
				float mediumNoise = morphNoise3D(
					ux * 3.35f, uy * 3.35f, uz * 3.35f,
					firstNoiseWeight, secondNoiseWeight, thirdNoiseWeight, 1u
				);
				float fineNoise = morphNoise3D(
					ux * 6.20f, uy * 6.20f, uz * 6.20f,
					firstNoiseWeight, secondNoiseWeight, thirdNoiseWeight, 2u
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
					fabsf(deformation) * 2.4f + furrow * 0.45f
				);
				vertex->alpha = clamp01(
					0.60f + vertex->flow * 0.24f
					- vertex->morphAmount * 0.055f
				);
			}
		}

		/*
		 * Normalen aus der wirklich verformten Flaeche berechnen. Erst dadurch
		 * werden die morphenden Ausbuchtungen als Licht und Schatten sichtbar.
		 */
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

	void drawMorphVertex(int latitudeBand, int longitudeSegment)
	{
		const MorphVertex* vertex =
			&gMorphMesh[latitudeBand][longitudeSegment];
		float diffuse = clamp01(
			-vertex->nx * 0.30f
			+ vertex->ny * 0.48f
			+ vertex->nz * 0.82f
		);
		float light = 0.18f + diffuse * 0.82f;
		float sheen = clamp01(
			-vertex->nx * 0.34f
			+ vertex->ny * 0.54f
			+ vertex->nz * 0.77f
		);
		float rim = clamp01(1.0f - fabsf(vertex->nz));
		float shapeShade =
			1.0f - vertex->morphAmount * (1.0f - diffuse) * 0.24f;

		sheen *= sheen;
		sheen *= sheen;
		sheen *= sheen;
		rim *= rim;

		glColor4f(
			clamp01((0.025f + (1.0f - vertex->flow) * 0.075f)
				* light * shapeShade + sheen * 0.30f + rim * 0.015f),
			clamp01((0.20f + vertex->flow * 0.52f)
				* light * shapeShade + sheen * 0.48f + rim * 0.075f),
			clamp01((0.065f + (1.0f - vertex->flow) * 0.23f)
				* light * shapeShade + sheen * 0.28f + rim * 0.11f),
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

	void drawLiquidSphere(
		float time,
		float gather,
		float rotationAngle
	)
	{
		float collapseScale = 1.0f - gather;
		if (collapseScale <= 0.003f)
			return;

		float pulse =
			1.0f
			+ 0.14f * sinf(time * 1.55f)
			+ 0.050f * sinf(time * 3.10f + 0.70f);
		float radius = 0.205f * pulse * collapseScale;
		float horizontalScale =
			0.76f * (1.0f + 0.075f * sinf(time * 1.55f + 0.40f));

		buildMorphMesh(time, radius, horizontalScale, rotationAngle);

		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		for (int latitudeBand = 0;
			latitudeBand < kMorphLatitudeBands;
			latitudeBand++)
		{
			glBegin(GL_TRIANGLE_STRIP);
			for (int longitudeSegment = 0;
				longitudeSegment <= kMorphLongitudeSegments;
				longitudeSegment++)
			{
				drawMorphVertex(latitudeBand, longitudeSegment);
				drawMorphVertex(latitudeBand + 1, longitudeSegment);
			}
			glEnd();
		}

		/* Helle Kernkante plus weicher gruener Schein um die Silhouette. */
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
		glCullFace(GL_FRONT);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		drawMorphGlowShell(1.075f, 0.12f);
		drawMorphGlowShell(1.040f, 0.36f);
		glDepthMask(GL_TRUE);
		glCullFace(GL_BACK);

		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}

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
	/* Erst im letzten Frame ganz zusammenfallen, ohne Schwarzblende. */
	float gather = smoothStep((animTime - gatherStart) / 4.0f);
	float fade = 1.0f;

	/* Weniger Sterne = spuerbar billiger (2x Loops: Lines + Points) */
	const int starCount = 220;
	const float speed = 0.27f;

	/* Leuchtspuren zeigen Flugrichtung und zunehmenden Wirbel. */
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

		/* Beim Zuruecksetzen in die Ferne keine Bildschirmdiagonale ziehen. */
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

	/* Helle Koepfe auf den Spuren. */
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
