##
## This is part of the Oculus Unreal Forward Renderer.
##
## You can use it to generate the MSAA resolve filters (WideCustomResolve_{Wide,Wider,Widest}.usf) by
## setting the desired parameters, running it and replacing the file contents (once each for each width).
##
## The default settings are quite conservative: the filter isn't too large, and we discard samples
## with small weights.  The result is improved antialiasing (compared to a box-filter resolve) while
## keeping the image fairly sharp.
## The resolve filter can have a distinct effect on your final frame, please experiment with a few
## settings and various filters.  We've found for our art styles that sharpening filters (those with
## negative weights) generally are undesirable, but this may not be the case for your content.
##
## A great blog post with a ton more investigation than we were able to perform:
## https://mynameismjp.wordpress.com/2012/10/28/msaa-resolve-filters/
##
##

import math

def sinc(x):
	if x == 0:
		return 1
	else:
		return math.sin(math.pi * x) / (math.pi * x)

def lanczos1D(x, a):
	if -a < x < a:
		return sinc(x) * sinc(x/a)
	else:
		return 0

def gaussian1D(x, stdDev):
	return math.exp(-x**2 / (2 * stdDev**2)) / math.sqrt(2 * math.pi * stdDev**2)

def gaussian(x, y, stdDev):
	return gaussian1D(x, stdDev) * gaussian1D(y, stdDev)

def lanczos(x, y, a):
	return lanczos1D(x, a) * lanczos1D(y, a)

def cubic(x, B, C):
	x *= 2.0

	y = 0.0
	x2 = x*x
	x3 = x*x*x
	if x < 1:
		y = (12 - 9*B - 6*C)*x3 + (-18 + 12*B + 6*C)*x2 + (6 - 2*B)
	elif x < 2:
		y = (-B - 6*C)*x3 + (6*B + 30*C)*x2 + (-12*B - 48*C)*x + (8*B + 24*C)
	return y / 6.0

def bspline(x, y, a):
	l = math.sqrt(x*x + y*y) / a
	return cubic(l, 1.0, 0.0)

# See glMultisamplefv()
MSAAPatterns = {1: [( 0, 0,)],
 
				2: [( 0.250,  0.250,),
			        (-0.250, -0.250,)],

				4: [(-0.125, -0.375,), 
					( 0.375, -0.125,), 
					(-0.375,  0.125,),
					( 0.125,  0.375,)],

				8: [( 0.0625, -0.1875,),
				    (-0.0625,  0.1875,), 
					( 0.3125,  0.0625),
					(-0.1875, -0.3125,),
					(-0.3125,  0.3125,),
					(-0.4375, -0.0625,),
					( 0.1875,  0.4375,), 
					( 0.4375, -0.4375,)]
				}



def writeFilterWeights (filter, sampleCount, width, cutoff):
	import math
	print ("")
	print ("// filter={filter}, r={r} with cutoff={cutoff}".format(filter=filter.__name__, r=width, cutoff=cutoff))
	print ("float3 resolve_{}(uint2 pos)".format(filter.__name__))
	print ("{")

	n = 0
	culled = 0
	print ("    float3 sample = 0;")
	print ("    float3 sampleSum = 0;")
	print ("    float sampleW = 0;")
	print ("    float weightSum = 0;")
	totalWeight = 0.0
	iWidth = int(math.ceil(width))
	for y in range (-iWidth, iWidth+1):
		for x in range (-iWidth, iWidth+1):
			for sample in range (sampleCount):
				samplePosition = MSAAPatterns [sampleCount][sample]
				weight = filter (samplePosition[0]+x, samplePosition[1]+y, width)
				if abs (weight) > cutoff:
					print ("")
					print ("    sample = Tex.Load(pos + uint2({x}, {y}), {s}).xyz;".format (
						n = n, x = x, y = y, s = sample, w = weight))
					print ("    sampleW = CalcSampleWeight({w}, sample);".format(w=weight))
					print ("    sampleSum += sample*sampleW;")
					print ("    weightSum += sampleW;")
					n += 1
					totalWeight += weight
				else:
					culled += 1
	print ("")
	print ("    // {} samples".format(n))
	print ("    return sampleSum / weightSum;")
	print ("}")


print ("//")
print ("// This file has been automatically generated")
print ("//")

# TODO: add support for 8x MSAA
#for i in [2, 4, 8]:
for i in [2, 4]:

	print ("")
	print ("#if MSAA_SAMPLE_COUNT == {}".format(i))
	print ("")
	print ("Texture2DMS<float4,{}> Tex; // Input MSAA color".format(i))

	# controls which samples (with small weights) to discard
	cutoff = 2.0/255.0
	
	# customize with your filter function and widths!
	#writeFilterWeights(lanczos, i, 1.6, cutoff)
	#writeFilterWeights(gaussian, i, 0.5, cutoff)
	writeFilterWeights(bspline, i, 1.25, cutoff)

	print("\n#endif /* {}xMSAA */".format(i))

print ("")
