
//BUILD: g++ `pkg-config --cflags opencv` beadando.cpp `pkg-config --libs opencv` -o beadando


#include <iostream>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

void invert_img(Mat& img){
	img = Scalar(255, 255, 255) - img;	//invertáló függvény
}

float findAngle(vector<vector<Point>> contours, int i){
	float angle;
	
    cv::RotatedRect rotatedRect = cv::minAreaRect(contours[i]);

    cv::Point2f rect_points[4]; 
    rotatedRect.points( rect_points );
    angle = rotatedRect.angle; 													//megkeressük az elforgatás fokát és próbáljuk korrigálni
    if(angle > 89 && angle < 91)
    	angle = 0;
    else if (angle < -89 && angle > -91)
    	angle = 0;																//"kerekítés" (bugfix) 
   	return angle;
}

void rotateImage(Mat& img, float angle){
	if (angle < -45){
		angle = 90+angle;														//bugfix 

	}
    cv::Point2f center((img.cols-1)/2.0, (img.rows-1)/2.0);
    cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);

    cv::Rect2f bbox = cv::RotatedRect(cv::Point2f(), img.size(), angle).boundingRect2f();	//a fent kiszámolt szöggel való forgatás

    rot.at<double>(0,2) += bbox.width/2.0 - img.cols/2.0;
    rot.at<double>(1,2) += bbox.height/2.0 - img.rows/2.0;

    cv::warpAffine(img, img, rot, bbox.size());


}

void cropImage(Mat& in,Mat& out, String mode){
	if(mode == "INV"){
		invert_img(in);
	}
	Rect boundRect;
	bool flag = true;
   	
   	while(flag == true){
		float angle;
		vector<vector<Point>> contours;

		findContours(in, contours,  CV_RETR_TREE, CHAIN_APPROX_SIMPLE);
		int maxi = 0;
		vector<vector<Point> > contours_poly( contours.size() );
	   	double maxArea = 0.0;
	   	for( int i = 0; i < contours.size(); i++ ){ 
	        double area = contourArea(contours[i]);
	        if(area > maxArea) {																	//kivágja a legnagyobb területű kontúrt, így 
	            maxArea = area;
	            maxi = i;																	//közel azonos méretű tesztlapokat kapunk 
	            approxPolyDP( Mat(contours[i]), contours_poly[i], 0, false );				// (a táblázat széle)
	            boundRect = boundingRect( Mat(contours_poly[i]) );							//integrált invertálással
	        }
	    }

	    angle = findAngle(contours, maxi);
	    if(angle != 0){
	    
	    	rotateImage(in, angle);
	   	}
	    else {
	     flag = false;
	 	}
	 
	}
    out = in(boundRect);


    if(mode == "INV"){
		invert_img(out);
	}

}

void deleteTable(Mat& img, String mode){
	if(mode == "INV"){
		invert_img(img);
	}

	for(int i = 0; i < img.rows; i++){
   		for(int j=0; j < img.cols; j++){
      
	     	if(img.at<uchar>(i,j) < 100){
	        	img.at<uchar>(i,j) = 255;										//eltávolítja a fekete pixeleket a képről
	      	}																	//(a mintában a táblázatrács eltűntetése)
	   }																		
	}
	if(mode == "INV"){
		invert_img(img);
	}	
}

int main(int argc, char** argv) {
	Mat solv = imread("./tesztek/test_solver.png", IMREAD_GRAYSCALE );

	cropImage(solv, solv, "INV");										//csak a táblázat megtartása


	deleteTable(solv, "NORMAL");	 									//cellahatárok törláse
	Mat kernel=cv::getStructuringElement( MORPH_OPEN, Size(6, 6));
	dilate(solv, solv, kernel);
	
	threshold(solv, solv, 100, 255, THRESH_OTSU);						//az esetleg megmaradt bordereket dilatáljuk, küszöbölés

	kernel=cv::getStructuringElement( MORPH_OPEN, Size(4, 4));
	erode(solv, solv, kernel);
	erode(solv, solv, kernel);
	dilate(solv, solv, kernel);	
	dilate(solv, solv, kernel);											//kiváltható lenne 1-1 használattal, de nehéz volt megtalálni az 
	dilate(solv, solv, kernel);											//egyensúlyt szóval "black magic don't touch"

	int test_num = 0;
	int i = 0; //iterátor a for-hoz
	if (argc < 2 ){
		test_num = 18;
		cout<<"Nincs argumentum, az összes teszt kiértékelése..."<<endl;	//az argumentumokkal megadható hány kép legyen kiértékelve
	}
	else if (argc > 2) {
		i = atoi(argv[2]);
		test_num = i+atoi(argv[1]);										//és az hogy hanyadik képtől kezdje
	}
	else {
	 	test_num = atoi(argv[1]);
		i = 0;
	}
	 cout<<"Automatikus tesztkiértékelő program v1.1\n\n A teszten 10 pont érhető el. Az eredmények:\n"<<endl;

	for ( i; i < test_num; i++) {
		
		Mat test = imread("./tesztek/test_"+to_string(i)+".png", IMREAD_GRAYSCALE);

		//imshow("   ", test);
		threshold(test, test, 100, 255, THRESH_OTSU);					//küszöbölés
		
		cropImage(test, test, "INV");
		invert_img(test);												//körbevágás, átméretezés, maszkolás
		resize(solv, solv, test.size());


		test.setTo(0, solv);

		
		kernel=cv::getStructuringElement( MORPH_OPEN, Size(8, 8));
		dilate(test, test, kernel);										//dilatált x-ek hogy csak egyszer találja meg a kontúrkereső
		vector<vector<Point> > contours; 
		findContours( test, contours, noArray(), RETR_LIST, CHAIN_APPROX_NONE );
		Mat drawning = test.clone();
		drawning.setTo(0);
		drawContours(drawning, contours, -1, 255, 3);					//a kontúrok száma a maszkolt képen lévő helyes x-ek száma is.
		double percent = contours.size()*10;
		cout<<to_string(i)+". teszt eredménye: "<<contours.size()<<"/10 ------------------ "+to_string(percent)+"%\n"<<endl;

		
		//imshow("test"+to_string(i), drawning);
		//waitKey();		
		
	}
	
	return 0;
}