package com.autonavi.decode;

public class H265ToJpeg {

	static final String libPath1 = "/home/lxq271332/H265ToJpeg/lib/ffmpeg/x86_64_shared/libavcodec.so";
	static final String libPath2 = "/home/lxq271332/H265ToJpeg/lib/ffmpeg/x86_64_shared/libavdevice.so";
	static final String libPath3 = "/home/lxq271332/H265ToJpeg/lib/ffmpeg/x86_64_shared/libavfilter.so";
	static final String libPath4 = "/home/lxq271332/H265ToJpeg/lib/ffmpeg/x86_64_shared/libavformat.so";
	static final String libPath5 = "/home/lxq271332/H265ToJpeg/lib/ffmpeg/x86_64_shared/libavutil.so";
	static final String libPath6 = "/home/lxq271332/H265ToJpeg/lib/ffmpeg/x86_64_shared/libpostproc.so";
	static final String libPath7 = "/home/lxq271332/H265ToJpeg/lib/ffmpeg/x86_64_shared/libswresample.so";
	static final String libPath8 = "/home/lxq271332/H265ToJpeg/lib/ffmpeg/x86_64_shared/libswscale.so";
	static final String libPath9 = "/home/lxq271332/H265ToJpeg/build/libH265ToJpeg.so";
	
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
