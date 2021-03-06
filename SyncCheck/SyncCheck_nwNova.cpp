/***********************************************************************************
 >  *        File:   SyncCheck.cpp                                                 *
 >  * Description:   This file uses Midas Spy function to take data from a 	   *
 >		     DataSink in MIDAS format and look for timestamps coincidences *
 >		     from the tree sources.					   *
 >		     When find a single coincidence fill a 1D histogram,           *
 >		     when find a double coincidence fill a 2D histogram            *
 >		     For the embeded canvas is "inspired" in an example of         *
 >			embstat.C from  Ilka Antcheva (CERN/EP/SFT)                *
 >  *      Author:   Jorge Agramunt Ros  IFIC/CSIC spain                           *
 >  *Site Contact:   agramunt@ific.uv.es                                           *
 >  * Last Change:   24/02/2015                                                    *
 >  *******************************************************************************/

#include <TApplication.h>
//#include <TQApplication.h>
#include <TGClient.h>
#include <TGButton.h>
#include <TRootEmbeddedCanvas.h>
#include <TGStatusBar.h>
#include <TCanvas.h>
#include <string>
#include <TF1.h>
#include <TH2F.h>
#include <TH1F.h>
#include <TStyle.h>
#include <TRandom.h>
#include <RQ_OBJECT.h>
#include <TTimer.h>
#include <TRootContextMenu.h>
#include "SyncCheck.h"
#include "syncSpyClass.hpp"
#include<stdio.h>
#include<thread>
#include<vector>
#include <utility>      // std::pair
#include <map>
#include <algorithm>    // std::for_each
#include <mutex>    // std::for_each
#include <time.h>       /* time_t, struct tm, difftime, time, mktime */
#define BUFFERSIZE 1024
        std::vector<int> X;
        std::vector<int> Y;
std::mutex mtx;           // mutex for critical section
#define Debug1
//#define Debug3
//#define Debug2

int Master,NoMaster1,NoMaster2;

extern "C"{
#include"dataspy.h"
}
std::mutex Spymtx[3];           // mutex for critical section

struct DataStruct{
uint32_t SourceId;
uint32_t DataId;
uint32_t E;
uint32_t PCtime;
uint32_t Aux1;
uint32_t Aux2;
uint32_t Aux3;
uint32_t Mark;
uint32_t TS;

};

void Help(char *progname) {
	std::cerr<<"SyncCheck  -M [Id of the master setup, 0,1,2 default 0]"<<std::endl;
    exit(1);
}


#define MAPTIME 2000
void Spy(std::vector<uint64_t> & v,std::vector<DataStruct> & e,int & vid,const int id){
  SpyDataSink MyDataSink;
  if(id==0)
    MyDataSink= SpyDataSink( "/dev/shm/SHM_110205",BUFFERSIZE);
  else if(id==1)
    MyDataSink= SpyDataSink( "/dev/shm/SHM_110206",BUFFERSIZE);
  else if(id==2)
    MyDataSink= SpyDataSink( "/dev/shm/SHM_110207",BUFFERSIZE);
  
//  MyDataSink.Test();
  while(1){
            dataSpyVerbose(0);

           dataSpyOpen (id);

        int x=0,k=0;
        uint32_t buffer32 [BUFFERSIZE];
	//	std::vector<uint32_t> buffer32;
	        memset(buffer32,0,BUFFERSIZE);
 	std::cerr<<"kk en.. Id "<<std::dec<< id<<std::endl;
	  int debn=0;
      bool HaveData=false;
     while (x==0){
                  if (id==0)x= MyDataSink.Spy( (char*)buffer32);
                  else x= dataSpyRead (id, (char*)buffer32,BUFFERSIZE);
       //	  if(id==0)  x= MyDataSink.Spy( buffer32);
               int i;
                 if(x>0){ 
		  std::cerr<<"Id XXXXXXXXXXXXXXXXXXXXXXX"<<std::endl;
                        vid=buffer32[6];
                        Spymtx[id].lock();
                    HaveData=true;
std::cerr<<"+++++++++++++++++++++++++++++++++++++VId "<<vid<<std::endl;
                        for(i=8;i<BUFFERSIZE;i+=4){
                                if(buffer32[i]!=0){
                                        struct DataStruct Data;
                                        Data.PCtime=(uint32_t)time(NULL);
                                        Data.E=buffer32[i-1];
                                        Data.Mark=0;
                                        uint64_t TimeStamp;
					TimeStamp=buffer32[i+1];
                                        TimeStamp=TimeStamp<<32;
                                        TimeStamp+=buffer32[i];
                                        Data.TS=TimeStamp;
                                       v.push_back(TimeStamp);
                                        e.push_back(Data);
				        std::cerr<<"+----------------------------+Id "<<std::dec<< id<<" TimeStamp 0x"<<std::hex<<TimeStamp<<std::endl;

#ifdef Debug2
				        std::cerr<<"+----------------------------+Id "<<std::dec<< id<<" TimeStamp 0d"<<std::dec<<TimeStamp<<std::endl;
					//if(id==0)if(debn<100)std::cerr<<"Id "<<std::dec<< id<<" TimeStamp 0x"<<std::hex<<TimeStamp<<std::endl;
					debn++;
#endif

                                }
                                else break;

                        }
                         Spymtx[id].unlock();
			 sleep(1);
			 usleep(1000);
                }
                else{
                        usleep(100);
                        k++;
                }

                if(k>50000)x=1; // Wait for 5 seconds for data
        }
       //        dataSpyClose (id);
	//     if(HaveData)std::cerr<<"Id "<<std::dec<<id<<" recive data"<<std::endl;
	//     else std::cerr<<"Id "<<std::dec<<id<<" NO data"<<std::endl;

  }
}
void Merge(std::vector<int> & X,std::vector<int> & Y)
{

        short buffer16 [32*1024];
        int buffer32 [16*1024];
        long long buffer64 [8*1024];
  	time_t timer;
        int i,j,k;
        int x;
        int ID = 0;
        int d = 32;
        int v = 0;
        int DifCount=0;
        int NoDif=0;
        std::map <uint64_t,DataStruct> MergeMap;
        std::vector<std::thread> SpyTh;
        int MyId;
        std::vector <uint64_t> TSvect[3];
        std::vector <DataStruct> Evect[3];
        int j2;
        int TimeMapLength=MAPTIME;
        for(j2=0;j2<3;j2++){
                SpyTh.push_back(std::thread(Spy,std::ref(TSvect[j2]),std::ref(Evect[j2]),std::ref(MyId),j2));
        }

while(1){
usleep(10000);

        int NewTime=(int)time(NULL);
        int j2;
#ifdef Debug2
                std::cerr<<"Map size 0 "<<MergeMap.size()<<std::endl;
#endif
int vsize=0;

        for(j2=0;j2<3;j2++){
#ifdef Debug3
                //std::cerr<<"Spy...."<<std::endl;
#endif
                 if(TSvect[j2].size()>0){
                        Spymtx[j2].lock();
#ifdef Debug2
                         std::cerr<<"Vect sizess  -------------- "<<std::dec<<j2<< " "<<TSvect[j2].size()<<std::endl;
#endif

                        vsize+=TSvect[j2].size();

                        for(k=0;k<TSvect[j2].size();k++){
				std::pair<uint64_t, DataStruct> NewPair=std::make_pair((TSvect[j2][k]<<4)+j2,Evect[j2][k]);
				MergeMap.insert(NewPair);
//                                MergeMap.emplace((TSvect[j2][k]<<4)+j2,Evect[j2][k]);
//


                        }
                        Evect[j2].clear();
                        TSvect[j2].clear();
                        Spymtx[j2].unlock();
                }
        }
#ifdef Debug2
                std::cerr<<"Vect size 1 "<<vsize<<std::endl;
#endif
        if(vsize>0){
#ifdef Debug2
                std::cerr<<"Map size 1 "<<MergeMap.size()<<std::endl;
#endif
                int PrevId, NextId, PresentId;
                uint64_t NextTS , PrevTS , PresentTS ,PresentKey;

                for (std::map<uint64_t,DataStruct>::iterator itx= MergeMap.begin();itx!=  MergeMap.end();itx++){

                  if(itx->second.PCtime<(NewTime-TimeMapLength)){  // Erase elemnet too old
                        MergeMap.erase(itx->first);
                  }
                  else if((itx->second.Mark&0xf)==2){   // Erase element already used

                        MergeMap.erase(itx->first);
                  }
                }
//                  for (auto& itx: MergeMap){
                for (std::map<uint64_t,DataStruct>::iterator itx= MergeMap.begin();itx!=  MergeMap.end();itx++){


                        std::map<uint64_t,DataStruct>::iterator itlow , itup;
                        PrevTS=PresentTS;
                        PresentKey=itx->first;
                        PresentTS=(itx->first>>4);
                        PresentId=(itx->first&0xf);
#ifdef Debug2
        
			             std::cerr<<"Id "<< PresentId <<" TS 0x"<<std::hex<< PresentTS <<" TS 0d"<<std::dec<< PresentTS <<std::endl;
	
#endif


		if(PresentId==Master){
                        itlow=MergeMap.lower_bound (itx->first-10000000);  // itlow points to b
                        itup=MergeMap.upper_bound (itx->first+10000000);   // itup points to e (not d!)
                                bool OnlyOne=true;
                                std::vector<DataStruct> CoincidentData;
                                int Ncoinc=0;
                                std::vector<int> ToDoubleSend;
                                for (std::map<uint64_t,DataStruct>::iterator it=itlow; it!=itup; ++it){
                                        Ncoinc++;
                                }

                                for (std::map<uint64_t,DataStruct>::iterator it=itlow; it!=itup; ++it){


#ifdef Debug3
                                                std::cerr <<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << '\n';
                                                std::cerr <<"Master  0x"<<std::hex<< Master<<'\n';
                                                std::cerr <<"Present  0x"<<std::hex<< PresentId << " => 0x"<<std::hex << PresentTS << '\n';
                                                std::cerr <<"Compared 0x"<<std::hex<<  (it->first&0xf) << " => 0x"<<std::hex << (it->first>>4) << '\n';
                                                std::cerr <<"Diference "<< std::dec<< (int64_t) (it->first>>4) -  (int64_t)PresentTS  << '\n';
                                                 std::cerr <<"==========================================================================" << '\n';
#endif

                                if(Ncoinc==2){

                                        if(((it->first&0xf)!=PresentId) && ((it->second.Mark&0xf)==0)){
#ifdef Debug1
                                                std::cerr <<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << '\n';
                                                std::cerr <<"Master  0x"<<std::hex<< Master<<'\n';
                                                std::cerr <<"Present  0x"<<std::hex<< PresentId << " => 0x"<<std::hex << PresentTS << '\n';
                                                std::cerr <<"Compared 0x"<<std::hex<<  (it->first&0xf) << " => 0x"<<std::hex << (it->first>>4) << '\n';
                                                std::cerr <<"Diference "<< std::dec<< (int64_t) (it->first>>4) -  (int64_t)PresentTS  << '\n';
                                                 std::cerr <<"==========================================================================" << '\n';
#endif
                                                int Diff=(int) ((int64_t) (it->first>>4) -  (int64_t)PresentTS);

        					mtx.lock();

        					if((it->first&0xf) ==NoMaster1){
                					X.push_back(Diff);
							Y.push_back(0xFFFF);
        					}
        					else if((it->first&0xf) ==NoMaster2){
                					Y.push_back(Diff);
                					X.push_back(0xFFFF);

        					}
					        mtx.unlock();


                                                 it->second.Mark=it->second.Mark|0x00000001;        // Mark as 0xXXXX1
                                        }

                                }
                                if(Ncoinc==3){
                                        if((it->first&0xf)!=PresentId && ((it->second.Mark&0xf)==0)){// Send individual and mark as double sent
#ifdef Debug1
                                                std::cerr <<"s+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << '\n';
					        std::cerr <<"Master  0x"<<std::hex<< Master<<'\n';
                                                std::cerr <<"Present  0x"<<std::hex<< PresentId << " => 0x"<<std::hex << PresentTS << '\n';
                                                std::cerr <<"Compared 0x"<<std::hex<<  (it->first&0xf) << " => 0x"<<std::hex << (it->first>>4) << '\n';
                                                std::cerr <<"Diference "<< std::dec<< (int64_t) (it->first>>4) -  (int64_t)PresentTS  << '\n';
                                                 std::cerr <<"==========================================================================" << '\n';
#endif
                                                int Diff=(int) ((int64_t) (it->first>>4) -  (int64_t)PresentTS);

                                                 it->second.Mark=0x00000002;        // Mark as 0xXXXX1

        					 mtx.lock();

        					if((it->first&0xf) ==NoMaster1){
                					X.push_back(Diff);
                					Y.push_back(0xFFFFFF);
        					}
        					else if((it->first&0xf) ==NoMaster2){
                					Y.push_back(Diff);
                					X.push_back(0xFFFFFF);
        					}
        					mtx.unlock();
                                                ToDoubleSend.push_back((PresentId<<4)+(int)(it->first&0xf));
                                                ToDoubleSend.push_back(Diff);
                                        }

                                        else if((it->first&0xf)!=PresentId){      // Mark as double send and store to send double after

                                                int Diff=(int) ((int64_t) (it->first>>4) -  (int64_t)PresentTS);
                                                ToDoubleSend.push_back((PresentId<<4)+(int)(it->first&0xf));
                                                ToDoubleSend.push_back(Diff);

                                                 it->second.Mark=0x00000002;        // Mark as 0xXXXX1
                                        }
                                        else{      // Mark as double send 
                                                 it->second.Mark=0x00000002;        // Mark as 0xXXXX1
                                        }
                                  }
                                }
                                if(ToDoubleSend.size()==4){

#ifdef Debug1
                                                std::cerr <<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << '\n';

                                                std::cerr <<"Diference of "<< ToDoubleSend[0] <<" "<< std::dec<<ToDoubleSend[1]  << '\n';
                                                std::cerr <<"Diference of "<< ToDoubleSend[2] <<" "<< std::dec<<ToDoubleSend[3]  << '\n';

                                                 std::cerr <<"==========================================================================" << '\n';
#endif
					        mtx.lock();

      						  if(ToDoubleSend[0]==NoMaster1){
                					X.push_back(ToDoubleSend[1]);
                					Y.push_back(ToDoubleSend[3]);
        					}
        					else{
                					X.push_back(ToDoubleSend[1]);
                					Y.push_back(ToDoubleSend[3]);
        					}
        					mtx.unlock();
                                }

                 	}
                }

#ifdef Debug1
        std::cerr <<"Diff Count Good "<<std::dec<< DifCount  << '\n';
        std::cerr<<"Map size 3 "<<std::dec<<MergeMap.size()<<std::endl;
#endif
   }
  }

}




void MyMainFrame::AutoUpdate()
{

mtx.lock();
    if(PlotUpdate->IsOn()){
        timer->Start(100,  kFALSE);   // 2 seconds single-shot
        fprintf(stderr,"Auto Update Start\n");
    }
    else{
        timer->Stop();
        fprintf(stderr,"Auto Update Stop\n");
        
    }
	X.clear();
	Y.clear();
    mtx.unlock();
}

void MyMainFrame::DoDraw()
{
    
    
    // Draw a function
    
    Printf("Slot DoDraw()");
    Double_t x=0,y=0;
    h2->Fill(x,y);
    h2->Draw("COLZ");
    
//    static TF1 *f1 = 0;
//    if (f1) delete f1;
//    f1 = new TF1("f1", "sin(x)/x", 0, gRandom->Rndm()*10);
//    f1->SetFillColor(19);
//    f1->SetFillStyle(1);
//    f1->SetLineWidth(3);
//    f1->Draw();
    TCanvas *fCanvas = fEcan->GetCanvas();
    fCanvas->cd();
    fCanvas->Update();
}

void MyMainFrame::DoExit()
{
    Printf("Slot DoExit()");
    gApplication->Terminate(0);
}

// Widget methods here...
void MyMainFrame::SetStatusText(const char *txt, Int_t pi)
{
    // Set text in status bar.
    fStatusBar->SetText(txt,pi);
}

void MyMainFrame::EventInfo(Int_t event, Int_t px, Int_t py, TObject *selected)
{
    //  Writes the event status in the status bar parts
    //Printf("Slot EventInfo()");
    
    TCanvas *c = (TCanvas *) gTQSender;
    
    const char *text0, *text1, *text3;
    char text2[50];
    text0 = selected->GetTitle();
    SetStatusText(text0,0);
    text1 = selected->GetName();
    SetStatusText(text1,1);
    if (event == kKeyPress)
        sprintf(text2, "%c", (char) px);
    else
        sprintf(text2, "%d,%d", px, py);
    SetStatusText(text2,2);
    text3 = selected->GetObjectInfo(px,py);
    SetStatusText(text3,3);
    if (event == kButton1Down){
        printf("event = %d, px=%d, py=%d, obj = %s::%s , kk %s\n", event,px,py,
               selected->GetName(), selected->GetTitle(),text3);
    }
    
}
void MyMainFrame::SelectPixel(Int_t event, Int_t x, Int_t y, TObject *selected){
    // Handle mouse click events in the embedded canvas.
    
    //    const char *text0, *text1, *text3;
    //    if (event == kButton1Down){
    //        text3 = selected->GetObjectInfo(x,y);
    //        printf("event = %d, px=%d, py=%d, obj = %s::%s , kk %s\n", event,x,y,
    //               selected->GetName(), selected->GetTitle(),text3);
    //    }
}

void MyMainFrame::ClearDone(){
	 h2partial->Reset("ICES");
}
void MyMainFrame::Reset(){
 	h2partial->Reset("ICES");
 	h2->Reset("ICES");
 	h1X->Reset("ICES");
 	h1Y->Reset("ICES");
}
void MyMainFrame::TimerDone()
{
    // Draw a function
   int k3;
//    Printf("Slot DoDraw()");
if(X.size()>0){
  mtx.lock();
	int i;
	for(i=0;i<X.size();i++){
	  // if(X[i]==0xFFFF)h1Y->Fill(Y[i]);
	  // if(Y[i]==0xFFFF)h1X->Fill(X[i]);
	  //	  if(Y[i]!=0xFFFF)h1Y->Fill(Y[i]);
	  //	  if(X[i]!=0xFFFF)h1X->Fill(X[i]);
          //during run 
	  h1Y->Fill(Y[i]);
	  h1X->Fill(X[i]);

		 if(X[i]==0xFFFF) X[i]=0;
                 if(Y[i]==0xFFFF) Y[i]=0;

                
                if((X[i]!=0)&&(Y[i]!=0) ) h2->Fill(X[i],Y[i]);
                if((X[i]!=0)&&(Y[i]!=0) ) h2partial->Fill(X[i],Y[i]);
	}
	X.clear();
        Y.clear();

  mtx.unlock();
}
    TCanvas *fCanvas = fEcan->GetCanvas();
    fCanvas->cd(1)->cd(1);

    	h2->Draw("COLZ");
    fCanvas->cd(1)->cd(2);

        h2partial->Draw("COLZ");

	
    fCanvas->cd(2)->cd(1);
	        h1X->Draw();

    fCanvas->cd(2)->cd(2);
                h1Y->Draw();

    fCanvas->Update();
//
}
MyMainFrame::MyMainFrame(const TGWindow *p, UInt_t w, UInt_t h)
{
    // Create the main frame
    fMain = new TGMainFrame(p,w,h);
    
    // Create the embedded canvas
    fEcan = new TRootEmbeddedCanvas(0,fMain,1200,800);
    Int_t wid = fEcan->GetCanvasWindowId();
    TCanvas *myc = new TCanvas("MyCanvas", 500,400,wid);
	myc->Divide(1,2);
	myc->cd(1)->Divide(2,1);

	myc->cd(2)->Divide(2,1);
    fEcan->AdoptCanvas(myc);
    myc->Connect("ProcessedEvent(Int_t,Int_t,Int_t,TObject*)","MyMainFrame",this,
                 "EventInfo(Int_t,Int_t,Int_t,TObject*)");
    
    myc->Connect("ProcessedEvent(Int_t,Int_t,Int_t,TObject*)",
                 "MyMainFrame", this,
                 "SelectPixel(Int_t,Int_t,Int_t,TObject*)");
    
    
    timer = new TTimer();
    timer->Connect("Timeout()", "MyMainFrame",
                   this, "TimerDone()");
    timer2 = new TTimer();
    timer2->Connect("Timeout()", "MyMainFrame",
                   this, "ClearDone()");

    double ll1d= -100000;
    double ul1d= 100000;
    double ll2d= -70000;
    double ul2d= 70000;

   	if(Master==0){ 
    		h2=new TH2F("Check","Synchronization Check;RIBF-BRIKEN time difference        x40ns;AIDA-BRIKEN time difference                  x40ns",200, ll2d, ul2d, 200, ll2d, ul2d);
    		h2partial=new TH2F("CheckPartial","Partial;RBIF-BRIKEN time difference        x40ns;AIDA-BRIKEN time difference                  x40ns",200, ll2d, ul2d, 200, ll2d, ul2d);

    		h1X=new TH1F("Check","RIBF1-BRIKEN time difference;                                x40ns",4000, ll1d, ul1d);
    		h1Y=new TH1F("Check","AIDA2-BRIKEN time difference;                                x40ns",4000, ll1d, ul1d);
	}
	        else if(Master==1){
                h2=new TH2F("Check","Synchronization Check;AIDA-RIBF time difference        x40ns;BRIKEN-RIBF time difference                  x40ns",200, ll2d, ul2d, 200, ll2d, ul2d);
                h2partial=new TH2F("CheckPartial","Partial;AIDA-RIBF time difference        x40ns;BRIKEN-RIBF time difference                  x40ns",200, ll2d, ul2d, 200, ll2d, ul2d);

                h1X=new TH1F("Check","AIDA-RIBF time difference;                                x40ns",4000, ll1d, ul1d);
                h1Y=new TH1F("Check","BRIKEN-RIBF time difference;                                x40ns",4000, ll1d, ul1d);
        }
        else if(Master==2){
                h2=new TH2F("Check","Synchronization Check;RIBF-AIDA  time difference        x40ns;BRIKEN-AIDA  time difference                  x40ns",200, ll2d, ul2d, 200, ll2d, ul2d);
                h2partial=new TH2F("CheckPartial","Partial;RIBF-AIDA  time difference        x40ns;BRIKEN-AIDA  time difference                  x40ns",200, ll2d, ul2d, 200, ll2d, ul2d);

                h1X=new TH1F("Check","BRIKEN-AIDA time difference;                                x40ns",4000, ll1d, ul1d);
                h1Y=new TH1F("Check","RIBF-AIDA time difference;                                x40ns",4000, ll1d, ul1d);
        }

gStyle->SetOptStat(110110);

   fMain->AddFrame(fEcan, new TGLayoutHints(kLHintsTop | kLHintsLeft
                                             | kLHintsExpandX  | kLHintsExpandY,0,0,1,1));
    // status bar
    Int_t parts[] = {45, 15, 10, 30};
    fStatusBar = new TGStatusBar(fMain, 50, 10, kVerticalFrame);
    fStatusBar->SetParts(parts, 4);
    fMain->AddFrame(fStatusBar, new TGLayoutHints(kLHintsExpandX, 0, 0, 10, 0));
    
    // Create a horizontal frame containing two buttons
    TGHorizontalFrame *hframe = new TGHorizontalFrame(fMain, 200, 40);
    
 //   PlotUpdate = new TGCheckButton(hframe, "Auto Update",1);
    //     AutoUpdate->SetState(kButtonUp);
//    PlotUpdate->Connect("Clicked()", "MyMainFrame", this, "AutoUpdate( )");
//    hframe->AddFrame(PlotUpdate, new TGLayoutHints(kLHintsCenterX, 5, 5, 3, 4));
    
    
 //   TGTextButton *draw = new TGTextButton(hframe, "&Draw");
 //   draw->Connect("Clicked()", "MyMainFrame", this, "DoDraw()");
 //   hframe->AddFrame(draw, new TGLayoutHints(kLHintsCenterX, 5, 5, 3, 4));
 
    TGTextButton *reset = new TGTextButton(hframe, "&Reset");
    reset->Connect("Clicked()", "MyMainFrame", this, "Reset()");
    hframe->AddFrame(reset, new TGLayoutHints(kLHintsCenterX, 5, 5, 3, 4));
 
   
    TGTextButton *exit = new TGTextButton(hframe, "&Exit ");
    exit->Connect("Pressed()", "MyMainFrame", this, "DoExit()");
    hframe->AddFrame(exit, new TGLayoutHints(kLHintsCenterX, 5, 5, 3, 4));

//	std::thread(GetData,std::ref(X),std::ref(Y));   

    
    fMain->AddFrame(hframe, new TGLayoutHints(kLHintsCenterX, 2, 2, 2, 2));
    
    // Set a name to the main frame
    fMain->SetWindowName("Embedded Canvas Status Info");
    fMain->MapSubwindows();
          timer->Start(100,  kFALSE);   // 1 seconds single-shot
          timer2->Start(100000,  kFALSE);   // 1000 seconds single-shot
  
    // Initialize the layout algorithm via Resize()
    fMain->Resize(fMain->GetDefaultSize());
    
    // Map main frame
    fMain->MapWindow();
    
}


MyMainFrame::~MyMainFrame()
{
    // Clean up main frame...
    fMain->Cleanup();
    delete fEcan;
    delete fStatusBar;
    delete fMain;
}


void embstat()
{
    // Popup the GUI...
    
    new MyMainFrame(gClient->GetRoot(), 200, 200);
}

int main(int argc, char **argv) {
	Master=0;
                NoMaster1=1;
                NoMaster2=2;

	int i;
    
   if (argc >1) {
        for(i=1;i <argc;i++) {
            if ( (argv[i][0] == '-') || (argv[i][0] == '/') ) {
                switch(argv[i][1]) {
                    case 'M':
                        Master = atoi(argv[++i]);
                        break;
                    default:
                        Help(argv[0]);
                        break;
                }
            }
        }
    }
	if(Master==1){
		NoMaster1=2;
		NoMaster2=0;
	}
	else if (Master==2){
                NoMaster1=0;
                NoMaster2=1;
        }
	if(Master>2) Help(argv[0]);

std::cerr<< "Master "<< Master << std::endl;
std::cerr<< "NoMaster1 "<< NoMaster1 << std::endl;
std::cerr<< "NoMaster2 "<< NoMaster2 << std::endl;


  std::thread InputData(Merge,std::ref(X),std::ref(Y));   




    TApplication theApp("App",&argc,argv);
    embstat();
    theApp.Run();
    return 0;
}

