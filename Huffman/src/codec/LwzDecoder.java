package codec;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import io.BitSource;
import io.InsufficientBitsLeftException;
import models.Symbol;

public class LwzDecoder {
	InputStream _readFile;
	int[] _decodedValueArray;
	int _decodedValueArrayIndex;
	int _decodedValueArrayTotalSize;
	
	public LwzDecoder(File aFile) throws FileNotFoundException{
		_readFile = new FileInputStream(aFile);
	//	_decodedValueArray = new int[60000000];
		_decodedValueArrayIndex= 0;
		_decodedValueArrayTotalSize= 0;
		
	}
	
	
	public void outputFrame(int[][] frame, OutputStream out) 
			throws IOException {
		int width = frame.length;
		int height = frame[0].length;
		for (int y=0; y<height; y++) {
			for (int x=0; x<width; x++) {
				out.write(frame[x][y]);
			}
		}
	}

	public int[][] reconstructFrame(int[][] prior_frame ,int arrayStart) {
		int width = prior_frame.length;
		int height = prior_frame[0].length;
		int index = arrayStart;
		int[] pixelsArray = getDecodedValueArray();
		int[][] current_frame = new int[width][height];
		
		for(int y = 0;y<height;y++) {
			for( int x=0; x<width;x++){
				current_frame[x][y]= (prior_frame[x][y]+pixelsArray[index])%256;
				index++;
			}
		}
				
		return current_frame;
	}

	public InputStream getReadFile() {return _readFile;}
	public int[] getDecodedValueArray() {return _decodedValueArray;}
	public void setDecodedValueArray(int[] newArray) { _decodedValueArray = newArray;}
	public int getDecodedValueArrayIndex() {return _decodedValueArrayIndex;}
	public void setDecodedValueArrayIndex(int index) { _decodedValueArrayIndex = index;}
	public int getDecodedValueArrayTotalSize() {return _decodedValueArrayTotalSize;}
	public void setDecodedValueArrayTotalSize(int size) { _decodedValueArrayTotalSize = size;}
	
	


	
}
