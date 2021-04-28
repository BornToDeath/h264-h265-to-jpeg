package com.autonavi.socol.occtiltedserver.service;

public class H265DecodeService {
	
	static final String libDir = "/Users/lixiaoqing/Desktop/socol/H265ToJpeg/for 张冬冬/H265ToJpeg/lib/";
	static final String libPath1 = libDir + "libavcodec.so";
	static final String libPath2 = libDir + "libavdevice.so";
	static final String libPath3 = libDir + "libavfilter.so";
	static final String libPath4 = libDir + "libavformat.so";
	static final String libPath5 = libDir + "libavutil.so";
	static final String libPath6 = libDir + "libpostproc.so";
	static final String libPath7 = libDir + "libswresample.so";
	static final String libPath8 = libDir + "libswscale.so";
	static final String libPath9 = libDir + "libH265ToJpeg.so";
	
	static {
		System.load(libPath1);
		System.load(libPath2);
		System.load(libPath3);
		System.load(libPath4);
		System.load(libPath5);
		System.load(libPath6);
		System.load(libPath7);
		System.load(libPath8);
	}
	
	public static native boolean decode(String inputPath, String outputPath);
	
}
