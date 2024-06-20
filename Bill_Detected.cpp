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

        // ���� ����ϱ� ���� ��ü ����, ������ �Ű������� GPU ��� ����(CUDA �ʿ�)
        Inference inf("yolov8l_korea_banknote_v2.onnx", cv::Size(640, 640), "classes.txt", false);
        inf.setClasses({ "50000_b", "50000_f", "5000_b", "5000_f", "10000_b", "10000_f", "1000_b", "1000_f" });

        // ������ ��ü�� ���ͷ� ����
        // Detection ����ü�� Ŭ����ID, Ŭ���� �̸�, ��Ȯ��, cv::Scalar ��, cv::Rect ���� ����
        std::vector<Detection> output = inf.runInference(img);

        int detections = output.size();

        // ��ü�� 1�� �̻� Ž�� ������
        if (output.size() >= 1)
        {
            // ��ü�� 2�� �̻� Ž�� �Ǿ�����
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

                //cout << "��Ȯ�� : " << detection.confidence << "\n "<< detection.className << endl << AREA_1000<< endl;

                // ��Ȯ���� 0.8 �̻��϶�
                if (detection.confidence >= 0.8)
                {
                    cv::cvtColor(img, img_gray, cv::COLOR_BGR2GRAY);
                    cv::threshold(img_gray, thresh, THRESHOLD, 255, cv::THRESH_BINARY);

                    vector<vector<Point>> contours;
                    vector<Vec4i> hierarchy;

                    cv::findContours(thresh, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

                    // ����ū ����� ã������ ������ ���̰��� �ε��� ���� �����ϱ� ���� ����
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

                    // �ܼ�ȭ ������ ��ǥ ���� ����
                    vector<vector<Point>> approx(1);

                    // ��ü �ѷ��� 0.04�� ���� ���� ����
                    double epsilon = 0.04 * length;
                    approxPolyDP(contours[index], approx[0], epsilon, true);
                    setArea(contourArea(approx[0], false) * PIXEL_TO_MILLI);

                    //cout << "���� : " << arcLength(approx[0], true)<< endl;
                    //cout << "Ŭ���� : " << detection.className<< endl;

                    // ������
                    if (detection.class_id == BACK_50000 || detection.class_id == FRONT_50000)
                    {
                        setAreaPercent(getArea() / AREA_50000);
                        setIsBill("50000");
                    }
                    // ��õ��
                    else if (detection.class_id == BACK_5000 || detection.class_id == FRONT_5000)
                    {
                        setAreaPercent(getArea() / AREA_5000);
                        setIsBill("5000");
                    }
                    // ����
                    else if (detection.class_id == BACK_10000 || detection.class_id == FRONT_10000)
                    {
                        setAreaPercent(getArea() / AREA_10000);
                        setIsBill("10000");
                    }
                    // õ��
                    else if (detection.class_id == BACK_1000 || detection.class_id == FRONT_1000)
                    {
                        setAreaPercent(getArea() / AREA_1000);
                        setIsBill("1000");

                        //���Ϲ��� ��ü Ž�� �ڵ�
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
        // ��ü�� Ž������ ��������
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

