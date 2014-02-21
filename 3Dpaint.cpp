/*------------------------------------------------------------------
	kanji 座標上に hiro で絵をかくプログラム(+アニメーション)
			6th Program
------------------------------------------------------------------*/

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifndef __APPLE__
#include <GL/gl.h>
#include <GL/glut.h>
#else
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#endif
#include <AR/gsub.h>
#include <AR/video.h>
#include <AR/param.h>
#include <AR/ar.h>

#ifdef _DEBUG
	#pragma comment(lib,"libARd.lib")
	#pragma comment(lib,"libARgsubd.lib")
	#pragma comment(lib,"libARvideod.lib")
	#pragma comment(linker,"/NODEFAULTLIB:libcmtd.lib")
#else
	#pragma comment(lib,"libAR.lib")
	#pragma comment(lib,"libARgsub.lib")
	#pragma comment(lib,"libARvideo.lib")
	#pragma comment(linker,"/NODEFAULTLIB:libcmt.lib")
#endif

//
// Camera configuration.
//
#ifdef _WIN32
char			*vconf = "Data\\WDM_camera_flipV.xml";
#else
char			*vconf = "";
#endif

#define PTT_NUM				2

#define PTT1_MARK_ID		0					// パターン .hiro
#define PTT1_PATT_NAME		"Data/patt.hiro"
#define PTT1_SIZE			80.0

#define PTT2_MARK_ID		1					// パターン .kanji
#define PTT2_PATT_NAME		"Data/patt.kanji"
#define PTT2_SIZE			80.0

#define MAX_LINES			100
#define MOVE_BALL_FRAME		15.0 // 何フレームでボールが移動するか
#define BALL_POINT_ERROR	2.0 // 何ミリの誤差で終点についたと判断するか


int             xsize, ysize;
int             thresh = 100;
int             count = 0;
bool			isFirst[PTT_NUM];	// 出力のぶれ修正用
int				flag_check = 0;		// 's' の入力処理
int				flag_reset = 0;		// 'r' の入力処理
int				flag_hide = 0;		// 'h' の入力処理
int				flag_animation = 1;	// 'a' の入力処理
int				L_P_count = 0;		// 使用した LINES_POINT 構造体の数を把握
double			itrans2[3][4];		// マーカ2の逆行列

char           *cparam_name    = "Data/camera_para.dat";
ARParam        cparam;

typedef struct{
	char			*patt_name;				//パターンファイル名
	int             patt_id;				//パターンID
	int				mark_id;				//マーカID
	int				visible;				//検出
	double          patt_width;				//パターンの幅
	double          patt_center[2];			//パターンの中心座標
	double          patt_trans[3][4];		//座標変換行列
}PTT_T;

typedef struct{
	double next_sphere_xyz[3]; // 次回に表示する球の座標
	double movement[3]; // x,y,zの各移動量
}MOVE_BALL;

typedef struct{
	double	start_point[3];	// Line の始点座標 {x, y, z}
	double	end_point[3];	// Line の始点座標 {x, y, z}
	double	trans[3][4];	// 上のLineで使用した座標行列
	int		flag_after_change;	// 一度でもマーカ2座標に変換したなら'1'
	MOVE_BALL ball;			// 移動する球のアニメーションに関する情報
}LINES_POINT;

PTT_T object[PTT_NUM] = {
	{PTT1_PATT_NAME, -1, PTT1_MARK_ID, 0, PTT1_SIZE, {0.0,0.0}},
	{PTT2_PATT_NAME, -1, PTT2_MARK_ID, 0, PTT2_SIZE, {0.0,0.0}}
};

LINES_POINT points[MAX_LINES];

static void   init(void);
static void   cleanup(void);
static void   keyEvent( unsigned char key, int x, int y);
static void   mainLoop(void);
static void   draw( void );
void mySetLight( void );
void mySetMaterial( GLfloat R, GLfloat G, GLfloat B);

void myMatrix(double trans[3][4]);
void myPrintPoint(void);
void print_how_to_use(void);
void print_programmer(void);

int myCheck_appear(int before, int now);
void visible_log(int num, int *before);
void mySelectWire(int i);
void mySelectColor(GLfloat R, GLfloat G, GLfloat B);
double abusolute_double(double d); // double の絶対値に変換
int myCheck_disappear(int before, int now);
void myLineLoop(int s, int e);
void myMarkSquare (int patt_id);
void myCopyTrans (double s[3][4], double d[3][4]);

// 2つのマーカの距離を計算し，re_x, re_y, re_z のアドレスに格納
void mydistance(double patt1_trans[3][4], double patt2_trans[3][4], double *re_x, double *re_y, double *re_z);
void myChangeBase(int mark_id);
void myChangeBaseInv(int mark_id);
void myMoveBallInit(LINES_POINT *p);
void myMoveBall(LINES_POINT *p);		// 今回のメイン
void myMoveBall2(LINES_POINT *p, int num);

int main(int argc, char **argv)
{
	print_programmer();
	glutInit(&argc, argv);
	init();

    arVideoCapStart();
	print_how_to_use();
    argMainLoop( NULL, keyEvent, mainLoop );
	return (0);
}

void print_programmer(void){
	printf("Programmer : Tomoya Kiriyama\n\n");
}

void print_how_to_use(void){
	printf("\t【3D paint】\n");
	printf("使用マーカ： patt.hiro	patt.kanji\n");
	printf("\thiro で絵を描き，kanji で座標変換\n");
	printf("patt.hiro  : 見えはじめの点から，見えなくなった点まで線を引く\n"
		"patt.kanji : 自由に座標変換を行える\n"
		"           : 描いた線分にアニメーションを付けて表示\n");
	printf("\nCommand\n"
		"Esc : 終了\n"
		" a  : アニメーションの変更\n"
		" s  : 座標情報の表示\n"
		" u  : 基本仕様の再表示\n"
		" h  : kanji のマーカの跡の表示/非表示\n"
		" p  : 座標logの 出力\n");
}

void myPrintPoint(void){
	printf("X:%f Y:%f Z:%f || X:%f Y:%f Z:%f\n",
	object[0].patt_trans[0][3], object[0].patt_trans[1][3], object[0].patt_trans[2][3],
	object[1].patt_trans[0][3], object[1].patt_trans[1][3], object[1].patt_trans[2][3]);
}

static void   keyEvent( unsigned char key, int x, int y)
{
	int i;

    /* quit if the ESC key is pressed */
    if( key == 0x1b ) {
        printf("*** %f (frame/sec)\n", (double)count/arUtilTimer());
        cleanup();
        exit(0);
    }
	if (key == 's') {
		if(flag_check){
			printf("--------------(stop)------------------\n");
		}else{
			printf("--------------(start)-----------------\n");
		}
		flag_check = 1 - flag_check;
	}
	if (key == 'h'){
		flag_hide = 1;
	}
	if (key == 'u'){
		print_how_to_use();
	}
	if (key == 'p'){
		printf("\n=====(LINES_POINT)=====\n");
		for(i = 0; i < L_P_count; i++){
			printf("%d : start %f %f %f\n", i, points[i].start_point[0], points[i].start_point[1], points[i].start_point[2] );
			printf("   : end   %f %f %f\n\n", points[i].end_point[0], points[i].end_point[1], points[i].end_point[2]);
		}
		printf("=========================\n");
	}
	if (key == 'a'){
		flag_animation = 1 - flag_animation;
		for(i = 0; i < L_P_count; i++){
			myMoveBallInit( &points[i] );
		}
	}
}

/* main loop */
static void mainLoop(void)
{
    ARUint8         *dataPtr;
    ARMarkerInfo    *marker_info;
    int             marker_num;
    int             j, k;
	int				i;

    /* grab a vide frame */
    if( (dataPtr = (ARUint8 *)arVideoGetImage()) == NULL ) {
        arUtilSleep(2);
        return;
    }
    if( count == 0 ) arUtilTimerReset();
    count++;

    argDrawMode2D();
    argDispImage( dataPtr, 0,0 );

    /* detect the markers in the video frame */
    if( arDetectMarker(dataPtr, thresh, &marker_info, &marker_num) < 0 ) {
        cleanup();
        exit(0);
    }

    arVideoCapNext();

    /* check for object visibility */
   	for( i = 0; i < PTT_NUM; i++){
		k = -1;
	    for( j = 0; j < marker_num; j++ ) {
	        if( object[i].patt_id == marker_info[j].id ) {
	            if( k == -1 ) k = j;
	            else if( marker_info[k].cf < marker_info[j].cf ) k = j;
	        }	
	    }
		if( k == -1 ) {
			object[i].visible = 0;
			isFirst[i] = 1;
		}
		else{
			/* get the transformation between the marker and the real camera */
			if( isFirst[i]){
				arGetTransMat(&marker_info[k], object[i].patt_center, object[i].patt_width, object[i].patt_trans);
			}else{
				arGetTransMatCont(&marker_info[k], object[i].patt_trans, object[i].patt_center, object[i].patt_width, object[i].patt_trans);
			}
			object[i].visible = 1;
			isFirst[i] = 0;
			
			/* 追加 */
			if(i == PTT2_MARK_ID){
				arUtilMatInv( object[PTT2_MARK_ID].patt_trans, itrans2);
			}
		}
	}
	draw();

	if(flag_check){
		myPrintPoint();
	}
	argSwapBuffers();
}

static void init( void )
{
    ARParam  wparam;
	int i;
	
    /* open the video path */
    if( arVideoOpen( vconf ) < 0 ) exit(0);
    /* find the size of the window */
    if( arVideoInqSize(&xsize, &ysize) < 0 ) exit(0);
    printf("Image size (x,y) = (%d,%d)\n", xsize, ysize);

    /* set the initial camera parameters */
    if( arParamLoad(cparam_name, 1, &wparam) < 0 ) {
        printf("Camera parameter load error !!\n");
        exit(0);
    }
    arParamChangeSize( &wparam, xsize, ysize, &cparam );
    arInitCparam( &cparam );
    printf("*** Camera Parameter ***\n");
    arParamDisp( &cparam );

	for( i=0; i < PTT_NUM; i++){ 
		if( (object[i].patt_id = arLoadPatt(object[i].patt_name)) < 0 ) {
			printf("パターン読み込みに失敗しました!! %s\n", object[i].patt_name);
			exit(0);
		}
		isFirst[i] = 1;
	}

    /* open the graphics window */
    argInit( &cparam, 1.0, 0, 0, 0, 0 );
}

/* cleanup function called when program exits */
static void cleanup(void)
{
    arVideoCapStop();
    arVideoClose();
    argCleanup();
}

//行列変換の適用
void myMatrix(double trans[3][4]){
	double gl_para[16];

	argConvGlpara( trans, gl_para);
	glMatrixMode( GL_MODELVIEW);
	glLoadMatrixd( gl_para);
}

static void draw( void )
{
	static int before_visible[2] = {0, 0};
	static int Wire_count = -1;  // 出力する Wire の選択(0～4)
	static int flag_first1 = 0;  // 一度でも hiro が見つかったら立つ
	int i;
	static double begin[3];
	double wtrans[3][4];
	double gl_para[16];
		
	/* 3Dオブジェクトを描画するための準備 */
	argDrawMode3D();
	argDraw3dCamera(0, 0);

	/* Zバッファなどの設定 */
	glClear( GL_DEPTH_BUFFER_BIT );	// 陰面処理の設定
	glEnable( GL_DEPTH_TEST );		// 陰面処理の適用

	mySetLight();				// 光源の設定
	glEnable( GL_LIGHTING );	// 光源の適用

	/* マーカの出現と消失の処理 */

	/* マーカ1の処理 */
	if( myCheck_appear(before_visible[PTT1_MARK_ID], object[PTT1_MARK_ID].visible) ){
		begin[0] = object[PTT1_MARK_ID].patt_trans[0][3];
		begin[1] = object[PTT1_MARK_ID].patt_trans[1][3];
		begin[2] = object[PTT1_MARK_ID].patt_trans[2][3];

		points[L_P_count].flag_after_change = 0;
	}
	if( myCheck_disappear(before_visible[PTT1_MARK_ID], object[PTT1_MARK_ID].visible) ){
		myCopyTrans( object[PTT1_MARK_ID].patt_trans, points[L_P_count].trans);
		points[L_P_count].start_point[0] = begin[0] - object[PTT1_MARK_ID].patt_trans[0][3];
		points[L_P_count].start_point[1] = -( begin[1] - object[PTT1_MARK_ID].patt_trans[1][3] ); // Y軸はカメラに対して反転しているので
		points[L_P_count].start_point[2] = -( begin[2] - object[PTT1_MARK_ID].patt_trans[2][3] ); // Z軸はカメラに対して反転しているので
		for(i=0; i<3; i++){
			points[L_P_count].end_point[i] = 0.0;
		}
		myMoveBallInit( &points[L_P_count] );

		L_P_count++;
		if(MAX_LINES <= L_P_count){
			L_P_count = 0;
		}
	}


	
	/* マーカ2の出現と消失 */
	if( myCheck_appear(before_visible[PTT2_MARK_ID], object[PTT2_MARK_ID].visible) ){
		for(i=0; i < L_P_count; i++){
			arUtilMatMul( itrans2, points[i].trans, wtrans);
			myCopyTrans( wtrans, points[i].trans);
			
			points[i].flag_after_change = 1;
		}
	}
	if( myCheck_disappear(before_visible[PTT2_MARK_ID], object[PTT2_MARK_ID].visible) ){
		for(i=0; i < L_P_count; i++){
			arUtilMatMul( object[PTT2_MARK_ID].patt_trans, points[i].trans, wtrans);
			myCopyTrans( wtrans, points[i].trans );
		}
	}

	/* 3Dオブジェクトの描画 */

	/* 過去に描いた線分の描画 */
	if(flag_first1 && L_P_count >= 1 && !(object[PTT2_MARK_ID].visible) ){
		mySelectColor( 1.0, 0.0, 0.0); // 材質特性の設定
		glLineWidth( 5.0);
		for(i=0; i < L_P_count; i++){
			glPushMatrix(); // 念のため
				myMatrix( points[i].trans);
				myLineLoop(i, i+1);
			glPopMatrix();  // 念のため
		}
	}

	/* マーカ1(Hiro) が映ったときの処理 */
	if(object[PTT1_MARK_ID].visible){
		flag_first1 = 1;

		/* 現在書いている線分の描画 */
		myMatrix(object[PTT1_MARK_ID].patt_trans);
		mySelectColor( 1.0, 0.8, 0.0); // 材質特性の設定
		glLineWidth( 5.0);

		glBegin( GL_LINES );
			glVertex3f( begin[0] - object[PTT1_MARK_ID].patt_trans[0][3], -( begin[1] - object[PTT1_MARK_ID].patt_trans[1][3] ),
				-( begin[2] - object[PTT1_MARK_ID].patt_trans[2][3] ) );
			glVertex3f( 0.0, 0.0, 0.0 );
		glEnd();

		/* hiro のマーカを縁取り */
		myMatrix(object[PTT1_MARK_ID].patt_trans);
		mySelectColor( 0.0, 0.0, 1.0); // 材質特性の設定
		glLineWidth( 3.0);
		myMarkSquare (PTT1_MARK_ID);
	}

	/* マーカ2(kanji) が表示されているとき */
	
	if(object[PTT2_MARK_ID].visible){
		/* マーカ2の縁取り */
		myMatrix(object[PTT2_MARK_ID].patt_trans);
		mySelectColor( 0.0, 1.0, 0.0); // 材質特性の設定
		glLineWidth( 3.0);
		myMarkSquare (PTT2_MARK_ID);

		/* 座標変換 */
		glLineWidth( 5.0);

		for(i=0; i < L_P_count; i++){
			glPushMatrix();
				mySelectColor( 0.0, 0.0, 1.0); // 材質特性の設定
				argConvGlpara( points[i].trans, gl_para );
				glMultMatrixd( gl_para );
				myLineLoop(i, i+1);
				if( flag_animation){
					myMoveBall( &points[i] );
				}
				else{
					myMoveBall2( &points[i], i );
				}
			glPopMatrix();
		}
	}

	visible_log(PTT_NUM, before_visible);	// 各マーカの visible 状態の記録
	glDisable( GL_LIGHTING );	// 光源の無効化
	glDisable( GL_DEPTH_TEST );	// 陰面処理の無効化
}

// 光源の設定
void mySetLight(void){
	GLfloat light_diffuse[]	 = { 0.9, 0.9, 0.9, 1.0 }; // 拡散反射光
	GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 }; // 鏡面反射光
	GLfloat light_ambient[]  = { 0.3, 0.3, 0.3, 0.1 }; // 環境光
	GLfloat light_position[] = { 100.0, -200.0, 200.0, 0.0 }; // 位置と種類
	/* 光源の設定 */
	glLightfv( GL_LIGHT0, GL_DIFFUSE, light_diffuse );		// 拡散反射光の設定
	glLightfv( GL_LIGHT0, GL_SPECULAR, light_specular );	// 鏡面反射光の設定
	glLightfv( GL_LIGHT0, GL_AMBIENT, light_ambient );		// 環境光の設定
	glLightfv( GL_LIGHT0, GL_POSITION, light_position );	// 位置と種類の設定

	glShadeModel( GL_SMOOTH );	// シェーディングの種類と設定
	glEnable( GL_LIGHT0 );		// 光源の有効化
}

void mySetMaterial( GLfloat R, GLfloat G, GLfloat B){
	GLfloat mat_diffuse[]  = { R, G, B, 1.0 };	// 拡散光の反射係数
	GLfloat mat_specular[] = { R, G, B, 1.0 };	// 鏡面光の反射係数
	GLfloat mat_ambient[]  = { R, G, B, 1.0 };	// 環境光の反射係数
	GLfloat shininess[]    = { 50.0 };					// 鏡面光の指数

	/* 材質特性 */
	glMaterialfv( GL_FRONT, GL_DIFFUSE, mat_diffuse );		// 拡散反射の設定
	glMaterialfv( GL_FRONT, GL_SPECULAR, mat_specular );	// 鏡面反射の設定
	glMaterialfv( GL_FRONT, GL_AMBIENT, mat_ambient );		// 環境反射の設定
	glMaterialfv( GL_FRONT, GL_SHININESS, shininess );		// 鏡面反射の鋭さの設定
}

// 今のフレームでのマーカの visible情報を格納
void visible_log(int num, int *before){
	int i;

	for(i = 0; i < num; i++){
		*before = object[i].visible;
		before++;
	}
}


// 新しくマーカが出現したなら '1' を返す
int myCheck_appear(int before, int now){
	if(!before){
		if(now){
			return 1;
		}
	}
	return 0;
}

// マーカが見えなくなったなら '1' を返す
int myCheck_disappear(int before, int now){
	if(before){
		if(!now){
			return 1;
		}
	}
	return 0;
}

// 3Dオブジェクトを描画(引数によって変化)
void mySelectWire(int i){
	switch(i){
		case 0:
			glutWireSphere( 30.0, 10, 10);
			break;
		case 1:
			glutWireCube( 40.0 );
			break;
		case 2:
			glScalef( 1.5, 1.5, 1.5 );
			glutWireTorus( 10.0, 20.0, 20, 6);
			break;
		case 3:
			glScalef( 40.0, 40.0, 40.0 );
			glutWireTetrahedron();
			break;
		case 4:
			glTranslatef( 0.0, 0.0, 10.0 );
			glRotatef( 90.0, 1.0, 0.0, 0.0 );
			glutWireTeapot( 40.0 );
			break;
		default:
			glutWireIcosahedron();
			printf("EROOR: Wire_count\n");
			break;
	}
}

// 引数によって色を変化させる
void mySelectColor(GLfloat R, GLfloat G, GLfloat B){
	mySetMaterial( R, G, B);
}

// double の絶対値を返す
double abusolute_double(double d){
	if(d < 0.0)
		return -d;
	else
		return d;
}

// 2つのパットの距離を計算
void mydistance(double patt1_trans[3][4], double patt2_trans[3][4], double *re_x, double *re_y, double *re_z){
	*re_x = patt1_trans[0][3] - patt2_trans[0][3];
	*re_y = patt1_trans[1][3] - patt2_trans[1][3];
	*re_z = patt1_trans[2][3] - patt2_trans[2][3];
}

// 複数の線分を描写
void myLineLoop(int s, int e){
	int i;

	glBegin( GL_LINES );
		for( i=s; i < e; i++){
			glVertex3f( points[i].start_point[0], points[i].start_point[1], points[i].start_point[2] );
			glVertex3f( points[i].end_point[0], points[i].end_point[1], points[i].end_point[2] );
		}
	glEnd();
}

// マーカを縁取る正方形を描画する関数
void myMarkSquare (int patt_id){
	glBegin( GL_LINE_LOOP );
		glVertex3f( -object[patt_id].patt_width/2.0, -object[patt_id].patt_width/2.0, 0.0 );
		glVertex3f(  object[patt_id].patt_width/2.0, -object[patt_id].patt_width/2.0, 0.0 );
		glVertex3f(  object[patt_id].patt_width/2.0,  object[patt_id].patt_width/2.0, 0.0 );
		glVertex3f( -object[patt_id].patt_width/2.0,  object[patt_id].patt_width/2.0, 0.0 );
	glEnd();
}

// カメラ座標からマーカ座標への変換
void myChangeBase(int mark_id){
		double begin[3];
		int i;

		begin[0] = object[mark_id].patt_trans[0][3];
		begin[1] = object[mark_id].patt_trans[1][3];
		begin[2] = object[mark_id].patt_trans[2][3];
		
		for(i = 0; i < L_P_count; i++){
			points[i].start_point[0] = points[i].start_point[0] - begin[0];
			points[i].start_point[1] = -( points[i].start_point[1] - begin[1] ); // 座標軸が反転するので
			points[i].start_point[2] = -( points[i].start_point[2] - begin[2] ); // 座標軸が反転するので
			points[i].end_point[0]   = points[i].end_point[0] - begin[0];
			points[i].end_point[1]   = -( points[i].end_point[1] - begin[1] ); // 座標軸が反転するので
			points[i].end_point[2]   = -( points[i].end_point[2] - begin[2] ); // 座標軸が反転するので
		}
}

// マーカ座標からカメラ座標への変換
void myChangeBaseInv(int mark_id){
	double end[3];
	int i;

	end[0] = object[mark_id].patt_trans[0][3];
	end[1] = object[mark_id].patt_trans[1][3];
	end[2] = object[mark_id].patt_trans[2][3];

	for(i = 0; i < L_P_count; i++){
		points[i].start_point[0] = points[i].start_point[0] + end[0];
		points[i].start_point[1] = -points[i].start_point[1] + end[1]; // 座標軸が反転するので
		points[i].start_point[2] = -points[i].start_point[2] + end[2]; // 座標軸が反転するので
		points[i].end_point[0]   = points[i].end_point[0] + end[0];
		points[i].end_point[1]   = -points[i].end_point[1] + end[1]; // 座標軸が反転するので
		points[i].end_point[2]   = -points[i].end_point[2] + end[2]; // 座標軸が反転するので
	}
}

// 行列を s から d にコピーする
void myCopyTrans (double s[3][4], double d[3][4]){
	int i, k;

	for(i=0; i < 3; i++){
		for(k = 0; k < 4; k++){
			d[i][k] = s[i][k];
		}
	}
}

// LINES_POINT の MOVE_BALL型の中身の初期設定を行う
void myMoveBallInit(LINES_POINT *p){
	int i;
	
	for( i = 0; i < 3; i++){
		p->ball.next_sphere_xyz[i] = p->start_point[i];
		p->ball.movement[i] = ( p->start_point[i] - p->end_point[i] ) / MOVE_BALL_FRAME;
	}
}

// 移動する球のアニメーションを描画
void myMoveBall(LINES_POINT *p){
	int i;

	mySelectColor( 0.0, 1.0, 1.0); // 材質特性の設定
	
	glPushMatrix();
		glTranslatef( p->ball.next_sphere_xyz[0], p->ball.next_sphere_xyz[1], p->ball.next_sphere_xyz[2] );
		glutSolidSphere( 5.0, 10, 10 );
	glPopMatrix();
	
	/* 次の球の座標を更新 */
	for( i = 0; i < 3; i++){
		p->ball.next_sphere_xyz[i] -= p->ball.movement[i];
	}

	/* もしどれかが終端点までいったのなら座標をスタート地点にリセット */
	if( ( -BALL_POINT_ERROR < p->ball.next_sphere_xyz[0] && p->ball.next_sphere_xyz[0] < BALL_POINT_ERROR)
		&& ( -BALL_POINT_ERROR < p->ball.next_sphere_xyz[1] && p->ball.next_sphere_xyz[1] < BALL_POINT_ERROR)
		&& ( -BALL_POINT_ERROR < p->ball.next_sphere_xyz[2] && p->ball.next_sphere_xyz[2] < BALL_POINT_ERROR) ){
		for( i = 0; i < 3; i++){
			p->ball.next_sphere_xyz[i] = p->start_point[i];
		}
	}
}

void myMoveBall2(LINES_POINT *p, int num){
	static int i= 0;
	static int count = 0;

	if(num == i){
		myMoveBall( p );
		count++;
	}

	if(count == (int)MOVE_BALL_FRAME){
		i++;
		count = 0;
	}
	if(i == L_P_count){
		i = 0;
	}
}