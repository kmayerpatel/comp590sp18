package codec;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import io.BitSink;

public class LwzEncoder {
	
	private Map<String, String> _code_map;
	private boolean _closed;
	private int[] _parseArray;
	private int _parseArrayTotalSize;
	private int _parseArrayIndex;

	

	public LwzEncoder() {
		_code_map = new HashMap<String, String>();	
		_closed = false;
		_parseArray = new int[60000000];
		_parseArrayTotalSize=0;
		_parseArrayIndex = 0;
	}
	
	
	
	//Create code map
		public Map<String, String> createCodeMap(int[][] frame, Map<String, String> code_map){
			int width = frame.length;
			int height = frame[0].length;
			int value = 1000;
			String key = null;
			
			for(int x = 0 ; x<256;x++) {
				code_map.put(Integer.toString(x), Integer.toString(value));
				System.out.println("Added "+ x+ " to the Dictionary");	
				code_map.put(Integer.toString(value),Integer.toString(x));
				value++;		
			}
			
			for(int y = 0; y<height; y++) {
				for(int x = 0; x < width; x++) {
					key = Integer.toString(frame[x][y]);
					if(code_map.containsKey(key)) {
						while(code_map.containsKey(key)) {
							int counter = x;
							if (counter<width-1) {
								counter++;
							}else {
								if(y==height-1)break;		
								x=0;
								y+=1;
								counter = 0;
							}
							
							key += " "+ Integer.toString(frame[counter][y]);
							x=counter;
							
						}
						if(!code_map.containsKey(key)) {
							code_map.put(key,Integer.toString(value));
							System.out.println("Added "+ key+ " to the Dictionary");
							code_map.put(Integer.toString(value),key);
							value++;
						}
					}else if(!code_map.containsKey(key)) {
						code_map.put(key,Integer.toString(value));
						System.out.println("Added "+ key+ " to the Dictionary");
						code_map.put(Integer.toString(value),key);
						
						value ++;
					}
				}
			}
			return code_map;
		}
		
		public void encode(String s,BitSink out) throws IOException {
			if (_closed) {
				throw new RuntimeException("Attempt to encode symbol on closed encoder");
			}
			if (_code_map.containsKey(s)) {
				String stringToEncode = (_code_map.get(s));
				String stringToBinary = Integer.toBinaryString(Integer.parseInt(stringToEncode));
				System.out.println("encoded " + _code_map.get(stringToEncode) + " to " + stringToBinary);
				out.write(stringToBinary);
			} else {
				throw new RuntimeException("Symbol not in code map");
			}
		}
		
		public void close(BitSink out) throws IOException {
			out.padToWord();
			_closed = true;
		}

		
		public Map<String, String> getCodeMap() {return _code_map;}
		public int[] getParseArray() {return  _parseArray;}
		public int getParseArrayIndex() {return _parseArrayIndex;}
		public int getParseArrayTotalSize() {return _parseArrayTotalSize;}
		public void setParseArray(int[] array) {_parseArray =array;}
		public void setParseArrayIndex(int index) {_parseArrayIndex = index;}
		public void  setParseArrayTotalSize(int size) {_parseArrayTotalSize = size;}
		
	
}
