package apps;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.Map;

import models.Unsigned8BitModel;
import codec.HuffmanEncoder;

import codec.LwzDecoder;
import codec.LwzEncoder;
import codec.SymbolDecoder;
import codec.SymbolEncoder;
import models.Symbol;
import models.SymbolModel;
import models.Unsigned8BitModel.Unsigned8BitSymbol;
import sun.misc.IOUtils;
import io.InsufficientBitsLeftException;
import io.BitSink;
import io.BitSource;
import codec.ArithmeticDecoder;
import codec.ArithmeticEncoder;
import codec.HuffmanDecoder;
import io.InputStreamBitSource;
import io.OutputStreamBitSink;

//NAME: NATHAN LAM
//PID: 7300-12706
public class VideoApp {



	public static void main(String[] args) throws IOException, InsufficientBitsLeftException {
		String base = "bunny";
		String filename="/Users/kmp/tmp/" + base + ".450p.yuv";
		File file = new File(filename);
		int width = 800;
		int height = 450;
		int num_frames = 150;
		
		LwzEncoder encoder= new LwzEncoder();
		//LmzEncoder encoder= new LmzEncoder();
		Map<String, String> code_map = encoder.getCodeMap();
	
		InputStream training_values = new FileInputStream(file);
		int[][] current_frame = new int[width][height];

		//CREATE DICTIONARY
		for (int f=0; f < num_frames; f++) {
			System.out.println("Dictionary Looking Through Frame:" + f);
			int[][] prior_frame = current_frame;
			current_frame = readFrame(training_values, width, height);
			int[][] diff_frame = frameDifference(prior_frame, current_frame);	
			/////////////////////////////////////////////////////////////////
			encoder.createCodeMap(diff_frame,code_map);
		}
		training_values.close();		
		
		//ENCODE
		
		InputStream message = new FileInputStream(file);
		File out_file = new File("/Users/kmp/tmp/" + base + "-compressed.dat");
		OutputStream out_stream = new FileOutputStream(out_file);
		BitSink bit_sink = new OutputStreamBitSink(out_stream);

		current_frame = new int[width][height];

		for (int f=0; f < num_frames; f++) {
			System.out.println("Encoding frame difference " + f);
			int[][] prior_frame = current_frame;
			current_frame = readFrame(message, width, height);
			int[][] diff_frame = frameDifference(prior_frame, current_frame);
			//////////////////////////
			encodeFrameDifference(diff_frame, encoder, bit_sink);
		}
		message.close();
		encoder.close(bit_sink);
		out_stream.close();

		//DECODE
		
		BitSource bit_source = new InputStreamBitSource(new FileInputStream(out_file));
		OutputStream decoded_file = new FileOutputStream(new File("/Users/nathanlam/Downloads/" + base + "-decoded.dat"));
		LwzDecoder decoder = new LwzDecoder(out_file);
		
		
		int[] parseArray = encoder.getParseArray();
		String retrievedKey = null;
		decoder.getReadFile();
		
		int x = 0;
		int y = 0;
		current_frame = new int[width][height];
		int[][] prior_frame = new int[width][height];
		int framesDecoded = 0;
		
		for (int iteration = 0; iteration < encoder.getParseArrayIndex(); iteration++) {
			int value = bit_source.next(parseArray[iteration]);
		
			
			retrievedKey =   code_map.get(""+value);
			String[] pixels = retrievedKey.split(" ");;
			
			for(int o = 0; o<pixels.length; o++) {
				int pixel = Integer.parseInt(pixels[o]);
				current_frame[x][y]= (prior_frame[x][y]+pixel)%256;
				
			
				if( framesDecoded == num_frames) break;
				if(x== width-1) {
					x=0;
					if(y== height-1) {
						y=0;
						decoder.outputFrame(current_frame, decoded_file);
						System.out.println("Decoding Frame: "+ framesDecoded);
						framesDecoded++;
						prior_frame = current_frame;
					}else {
						y++;
					}
				}else {
					x++;	
				}
			}	
		}	
		decoded_file.close();

}



	
	
	
	private static int[][] readFrame(InputStream src, int width, int height) 
			throws IOException {
		int[][] frame_data = new int[width][height];
		for (int y=0; y<height; y++) {
			for (int x=0; x<width; x++) {
				frame_data[x][y] = src.read();
			}
		}
		return frame_data;
	}

	private static int[][] frameDifference(int[][] prior_frame, int[][] current_frame) {
		int width = prior_frame.length;
		int height = prior_frame[0].length;

		int[][] difference_frame = new int[width][height];

		for (int y=0; y<height; y++) {
			for (int x=0; x<width; x++) {
				difference_frame[x][y] = ((current_frame[x][y] - prior_frame[x][y])+256)%256;
			}
		}
		return difference_frame;
	}


	
	
	private static void encodeFrameDifference(int[][] frame, LwzEncoder encoder, BitSink bit_sink) 
			throws IOException {

 		int width = frame.length;
		int height = frame[0].length;
		Map<String, String> code_map = encoder.getCodeMap();
		String workingKey = Integer.toString(frame[0][0]);
		String testKey = workingKey;
		int counter = 0;
		int[] parseArray=encoder.getParseArray();
		int parseArrayIndex = encoder.getParseArrayIndex();
	
		for (int y=0; y<height; y++) {
			for (int x=0; x<width; x++) {
				counter =x;
				workingKey =Integer.toString(frame[x][y]);
				testKey = workingKey;
				if(code_map.containsKey(workingKey)) {
					while(code_map.containsKey(testKey)) {			
						if(counter==width-1) {
							y+=1;
							x=0;
							counter = x;
						}else {
							counter++;
						}
						workingKey =testKey;
						if(y<height) {
							testKey += " " + frame[counter][y];
						} else {	
							break;
						}			
					}
					
					if(y == height) {
						x = width;
					} else {
						x = counter - 1;
					}
					encoder.encode(workingKey,bit_sink);
					parseArray[parseArrayIndex]= Integer.toBinaryString(Integer.parseInt(code_map.get(workingKey))).length();
					parseArrayIndex++;
					encoder.setParseArrayIndex(parseArrayIndex);				
				}else if(!code_map.containsKey(workingKey)) {
					//will pass an error
					encoder.encode(workingKey,bit_sink);
				}		
			}
		}
		encoder.setParseArrayTotalSize(parseArrayIndex);
		encoder.setParseArrayIndex(parseArrayIndex);
		encoder.setParseArray(parseArray);
	}


}
