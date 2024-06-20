#pragma once
class Bill_Detected
{
private:
	Mat img;
	String isBill;
	double areaPercent;
	double area;

    // Setters
    void setImage(Mat Img);
    void setIsBill(const String& isBill);
    void setAreaPercent(double areaPercent);
    void setArea(double area);
public:
	Bill_Detected(vector<uchar>* pImgData);
    // 
    // Getters
    Mat getImage() const;
    String getIsBill() const;
    double getAreaPercent() const;
    double getArea() const;


};

