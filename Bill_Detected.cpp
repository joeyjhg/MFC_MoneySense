#include "pch.h"
#include "Bill_Detected.h"
#include "inference.h"

#define BACK_50000 0
#define FRONT_50000 1
#define BACK_5000 2
#define FRONT_5000 3
#define BACK_10000 4
#define FRONT_10000 5
#define BACK_1000 6
#define FRONT_1000 7
#define THRESHOLD 40
#define PIXEL_TO_MILLI 0.01166948898
#define AREA_1000 (68 * 136)
#define AREA_5000 (68 * 142)
#define AREA_10000 (68 * 148)
#define AREA_50000 (68 * 154)

Bill_Detected::Bill_Detected(vector<uchar>* pImgData) : isBill("-"), areaPercent(0.0), area(0.0)
{
    Mat img;
    img = cv::imdecode(*pImgData, cv::IMREAD_COLOR);
    //img = imread("image/12.jpg");
    if (!img.empty())
    {
        Mat img_gray;
        Mat thresh;

        // 모델을 사용하기 위해 객체 생성, 마지막 매개변수는 GPU 사용 여부(CUDA 필요)
        Inference inf("yolov8l_korea_banknote_v2.onnx", cv::Size(640, 640), "classes.txt", false);
        inf.setClasses({ "50000_b", "50000_f", "5000_b", "5000_f", "10000_b", "10000_f", "1000_b", "1000_f" });

        // 감지된 객체를 벡터로 저장
        // Detection 구조체는 클래스ID, 클래스 이름, 정확도, cv::Scalar 값, cv::Rect 값을 가짐
        std::vector<Detection> output = inf.runInference(img);

        int detections = output.size();

        // 객체를 1개 이상 탐지 했을때
        if (output.size() >= 1)
        {
            // 객체가 2개 이상 탐지 되었을때
            if (output.size() >= 2)
            {
                setImage(img);
                return;
            }

            for (int i = 0; i < detections; ++i)
            {
                Detection detection = output[i];

                cv::Rect box = detection.box;
                cv::Scalar color = detection.color;

                //cout << "정확도 : " << detection.confidence << "\n "<< detection.className << endl << AREA_1000<< endl;

                // 정확도가 0.8 이상일때
                if (detection.confidence >= 0.8)
                {
                    cv::cvtColor(img, img_gray, cv::COLOR_BGR2GRAY);
                    cv::threshold(img_gray, thresh, THRESHOLD, 255, cv::THRESH_BINARY);

                    vector<vector<Point>> contours;
                    vector<Vec4i> hierarchy;

                    cv::findContours(thresh, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

                    // 가장큰 컨투어를 찾기위해 컨투어 길이값과 인덱스 값을 저장하기 위한 변수
                    int length = 0, index = 0;

                    for (int i = 0; i < contours.size(); i++)
                    {
                        if (i == 0)
                            length = arcLength(contours[i], true);
                        if (arcLength(contours[i], true) > length)
                        {
                            length = arcLength(contours[i], true);
                            index = i;
                        }
                    }

                    // 단순화 컨투어 좌표 저장 벡터
                    vector<vector<Point>> approx(1);

                    // 전체 둘레의 0.04로 오차 범위 지정
                    double epsilon = 0.04 * length;
                    approxPolyDP(contours[index], approx[0], epsilon, true);
                    setArea(contourArea(approx[0], false) * PIXEL_TO_MILLI);

                    //cout << "길이 : " << arcLength(approx[0], true)<< endl;
                    //cout << "클래스 : " << detection.className<< endl;

                    // 오만원
                    if (detection.class_id == BACK_50000 || detection.class_id == FRONT_50000)
                    {
                        setAreaPercent(getArea() / AREA_50000);
                        setIsBill("50000");
                    }
                    // 오천원
                    else if (detection.class_id == BACK_5000 || detection.class_id == FRONT_5000)
                    {
                        setAreaPercent(getArea() / AREA_5000);
                        setIsBill("5000");
                    }
                    // 만원
                    else if (detection.class_id == BACK_10000 || detection.class_id == FRONT_10000)
                    {
                        setAreaPercent(getArea() / AREA_10000);
                        setIsBill("10000");
                    }
                    // 천원
                    else if (detection.class_id == BACK_1000 || detection.class_id == FRONT_1000)
                    {
                        setAreaPercent(getArea() / AREA_1000);
                        setIsBill("1000");

                        //볼록문자 객체 탐지 코드
                        /*Inference inf_1000won("yolov8l_1000F_v3.onnx", cv::Size(640, 640), "classes_1000.txt", false);
                        inf_1000won.setClasses({ "1000ld", "1000ru", "Governor", "chunWon", "koreaBank" });

                        std::vector<Detection> output_1000won = inf_1000won.runInference(img);

                        for (int j = 0; j < output_1000won.size(); ++j)
                        {
                            Detection detection_1000won = output_1000won[j];

                            cv::Rect box_1000won = detection_1000won.box;
                            cv::Scalar color_1000won = detection_1000won.color;
                            if (detection_1000won.confidence >= 0.5)
                            {
                                cv::rectangle(img, box_1000won, cv::Scalar(0,255,0), 2);
                            }
                        }*/
                    }

                    drawContours(img, approx, 0, Scalar(0, 255, 255), 2);

                    setImage(img);
                }
            }

            //cv::imshow("image", img);
            //waitKey(-1);

        }
        // 객체를 탐지하지 못했을때
        else
        {
            setImage(img);
        }
    }
}

Mat Bill_Detected::getImage() const
{
    return img;
}

String Bill_Detected::getIsBill() const
{
    return isBill;
}

double Bill_Detected::getAreaPercent() const
{
    return areaPercent;
}

double Bill_Detected::getArea() const
{
    return area;
}

void Bill_Detected::setImage(Mat Img)
{
    this->img = Img;
}

void Bill_Detected::setIsBill(const String& isBill)
{
    this->isBill = isBill;
}

void Bill_Detected::setAreaPercent(double areaPercent)
{
    this->areaPercent = areaPercent;
}

void Bill_Detected::setArea(double area)
{
    this->area = area;
}

