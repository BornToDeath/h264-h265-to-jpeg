package com.autonavi.decode;

public class Test {

	public static void main(String[] args) {

		String inputPath = "/home/lxq271332/H265ToJpeg/imgs/img02.h265";
		System.out.println("源文件：" + inputPath);

		String outputPath = "/home/lxq271332/H265ToJpeg/imgs/img02.jpeg";
		System.out.println("目标文件：" + outputPath);

		boolean res = H265ToJpeg.decode(inputPath, outputPath);
		if (res) {
			System.out.println("解码成功！");
		} else {
			System.out.println("解码失败！");
		}
	}
	
}
