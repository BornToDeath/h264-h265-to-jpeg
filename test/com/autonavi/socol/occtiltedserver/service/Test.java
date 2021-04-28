package com.autonavi.socol.occtiltedserver.service;

public class Test {

	public static void main(String[] args) {
		// TODO Auto-generated method stub

		String inputPath = "/Users/lixiaoqing/Desktop/socol/H265ToJpeg/for 张冬冬/H265ToJpeg/imgs/img01.h265";
		System.out.println("源文件：" + inputPath);		
		
		String outputPath = "/Users/lixiaoqing/Desktop/socol/H265ToJpeg/for 张冬冬/H265ToJpeg/imgs/img01.h265.jpeg";
		System.out.println("目标文件：" + outputPath);

		boolean res = H265DecodeService.decode(inputPath, outputPath);
		if (res) {
			System.out.println("解码成功！");
		} else {
			System.out.println("解码失败！");
		}
		
	}

}
