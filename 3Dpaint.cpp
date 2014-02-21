/*------------------------------------------------------------------
	kanji ���W��� hiro �ŊG�������v���O����(+�A�j���[�V����)
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

#define PTT1_MARK_ID		0					// �p�^�[�� .hiro
#define PTT1_PATT_NAME		"Data/patt.hiro"
#define PTT1_SIZE			80.0

#define PTT2_MARK_ID		1					// �p�^�[�� .kanji
#define PTT2_PATT_NAME		"Data/patt.kanji"
#define PTT2_SIZE			80.0

#define MAX_LINES			100
#define MOVE_BALL_FRAME		15.0 // ���t���[���Ń{�[�����ړ����邩
#define BALL_POINT_ERROR	2.0 // ���~���̌덷�ŏI�_�ɂ����Ɣ��f���邩


int             xsize, ysize;
int             thresh = 100;
int             count = 0;
bool			isFirst[PTT_NUM];	// �o�͂̂Ԃ�C���p
int				flag_check = 0;		// 's' �̓��͏���
int				flag_reset = 0;		// 'r' �̓��͏���
int				flag_hide = 0;		// 'h' �̓��͏���
int				flag_animation = 1;	// 'a' �̓��͏���
int				L_P_count = 0;		// �g�p���� LINES_POINT �\���̂̐���c��
double			itrans2[3][4];		// �}�[�J2�̋t�s��

char           *cparam_name    = "Data/camera_para.dat";
ARParam        cparam;

typedef struct{
	char			*patt_name;				//�p�^�[���t�@�C����
	int             patt_id;				//�p�^�[��ID
	int				mark_id;				//�}�[�JID
	int				visible;				//���o
	double          patt_width;				//�p�^�[���̕�
	double          patt_center[2];			//�p�^�[���̒��S���W
	double          patt_trans[3][4];		//���W�ϊ��s��
}PTT_T;

typedef struct{
	double next_sphere_xyz[3]; // ����ɕ\�����鋅�̍��W
	double movement[3]; // x,y,z�̊e�ړ���
}MOVE_BALL;

typedef struct{
	double	start_point[3];	// Line �̎n�_���W {x, y, z}
	double	end_point[3];	// Line �̎n�_���W {x, y, z}
	double	trans[3][4];	// ���Line�Ŏg�p�������W�s��
	int		flag_after_change;	// ��x�ł��}�[�J2���W�ɕϊ������Ȃ�'1'
	MOVE_BALL ball;			// �ړ����鋅�̃A�j���[�V�����Ɋւ�����
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
double abusolute_double(double d); // double �̐�Βl�ɕϊ�
int myCheck_disappear(int before, int now);
void myLineLoop(int s, int e);
void myMarkSquare (int patt_id);
void myCopyTrans (double s[3][4], double d[3][4]);

// 2�̃}�[�J�̋������v�Z���Cre_x, re_y, re_z �̃A�h���X�Ɋi�[
void mydistance(double patt1_trans[3][4], double patt2_trans[3][4], double *re_x, double *re_y, double *re_z);
void myChangeBase(int mark_id);
void myChangeBaseInv(int mark_id);
void myMoveBallInit(LINES_POINT *p);
void myMoveBall(LINES_POINT *p);		// ����̃��C��
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
	printf("\t�y3D paint�z\n");
	printf("�g�p�}�[�J�F patt.hiro	patt.kanji\n");
	printf("\thiro �ŊG��`���Ckanji �ō��W�ϊ�\n");
	printf("patt.hiro  : �����͂��߂̓_����C�����Ȃ��Ȃ����_�܂Ő�������\n"
		"patt.kanji : ���R�ɍ��W�ϊ����s����\n"
		"           : �`���������ɃA�j���[�V������t���ĕ\��\n");
	printf("\nCommand\n"
		"Esc : �I��\n"
		" a  : �A�j���[�V�����̕ύX\n"
		" s  : ���W���̕\��\n"
		" u  : ��{�d�l�̍ĕ\��\n"
		" h  : kanji �̃}�[�J�̐Ղ̕\��/��\��\n"
		" p  : ���Wlog�� �o��\n");
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
			
			/* �ǉ� */
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
			printf("�p�^�[���ǂݍ��݂Ɏ��s���܂���!! %s\n", object[i].patt_name);
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

//�s��ϊ��̓K�p
void myMatrix(double trans[3][4]){
	double gl_para[16];

	argConvGlpara( trans, gl_para);
	glMatrixMode( GL_MODELVIEW);
	glLoadMatrixd( gl_para);
}

static void draw( void )
{
	static int before_visible[2] = {0, 0};
	static int Wire_count = -1;  // �o�͂��� Wire �̑I��(0�`4)
	static int flag_first1 = 0;  // ��x�ł� hiro �����������痧��
	int i;
	static double begin[3];
	double wtrans[3][4];
	double gl_para[16];
		
	/* 3D�I�u�W�F�N�g��`�悷�邽�߂̏��� */
	argDrawMode3D();
	argDraw3dCamera(0, 0);

	/* Z�o�b�t�@�Ȃǂ̐ݒ� */
	glClear( GL_DEPTH_BUFFER_BIT );	// �A�ʏ����̐ݒ�
	glEnable( GL_DEPTH_TEST );		// �A�ʏ����̓K�p

	mySetLight();				// �����̐ݒ�
	glEnable( GL_LIGHTING );	// �����̓K�p

	/* �}�[�J�̏o���Ə����̏��� */

	/* �}�[�J1�̏��� */
	if( myCheck_appear(before_visible[PTT1_MARK_ID], object[PTT1_MARK_ID].visible) ){
		begin[0] = object[PTT1_MARK_ID].patt_trans[0][3];
		begin[1] = object[PTT1_MARK_ID].patt_trans[1][3];
		begin[2] = object[PTT1_MARK_ID].patt_trans[2][3];

		points[L_P_count].flag_after_change = 0;
	}
	if( myCheck_disappear(before_visible[PTT1_MARK_ID], object[PTT1_MARK_ID].visible) ){
		myCopyTrans( object[PTT1_MARK_ID].patt_trans, points[L_P_count].trans);
		points[L_P_count].start_point[0] = begin[0] - object[PTT1_MARK_ID].patt_trans[0][3];
		points[L_P_count].start_point[1] = -( begin[1] - object[PTT1_MARK_ID].patt_trans[1][3] ); // Y���̓J�����ɑ΂��Ĕ��]���Ă���̂�
		points[L_P_count].start_point[2] = -( begin[2] - object[PTT1_MARK_ID].patt_trans[2][3] ); // Z���̓J�����ɑ΂��Ĕ��]���Ă���̂�
		for(i=0; i<3; i++){
			points[L_P_count].end_point[i] = 0.0;
		}
		myMoveBallInit( &points[L_P_count] );

		L_P_count++;
		if(MAX_LINES <= L_P_count){
			L_P_count = 0;
		}
	}


	
	/* �}�[�J2�̏o���Ə��� */
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

	/* 3D�I�u�W�F�N�g�̕`�� */

	/* �ߋ��ɕ`���������̕`�� */
	if(flag_first1 && L_P_count >= 1 && !(object[PTT2_MARK_ID].visible) ){
		mySelectColor( 1.0, 0.0, 0.0); // �ގ������̐ݒ�
		glLineWidth( 5.0);
		for(i=0; i < L_P_count; i++){
			glPushMatrix(); // �O�̂���
				myMatrix( points[i].trans);
				myLineLoop(i, i+1);
			glPopMatrix();  // �O�̂���
		}
	}

	/* �}�[�J1(Hiro) ���f�����Ƃ��̏��� */
	if(object[PTT1_MARK_ID].visible){
		flag_first1 = 1;

		/* ���ݏ����Ă�������̕`�� */
		myMatrix(object[PTT1_MARK_ID].patt_trans);
		mySelectColor( 1.0, 0.8, 0.0); // �ގ������̐ݒ�
		glLineWidth( 5.0);

		glBegin( GL_LINES );
			glVertex3f( begin[0] - object[PTT1_MARK_ID].patt_trans[0][3], -( begin[1] - object[PTT1_MARK_ID].patt_trans[1][3] ),
				-( begin[2] - object[PTT1_MARK_ID].patt_trans[2][3] ) );
			glVertex3f( 0.0, 0.0, 0.0 );
		glEnd();

		/* hiro �̃}�[�J������� */
		myMatrix(object[PTT1_MARK_ID].patt_trans);
		mySelectColor( 0.0, 0.0, 1.0); // �ގ������̐ݒ�
		glLineWidth( 3.0);
		myMarkSquare (PTT1_MARK_ID);
	}

	/* �}�[�J2(kanji) ���\������Ă���Ƃ� */
	
	if(object[PTT2_MARK_ID].visible){
		/* �}�[�J2�̉���� */
		myMatrix(object[PTT2_MARK_ID].patt_trans);
		mySelectColor( 0.0, 1.0, 0.0); // �ގ������̐ݒ�
		glLineWidth( 3.0);
		myMarkSquare (PTT2_MARK_ID);

		/* ���W�ϊ� */
		glLineWidth( 5.0);

		for(i=0; i < L_P_count; i++){
			glPushMatrix();
				mySelectColor( 0.0, 0.0, 1.0); // �ގ������̐ݒ�
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

	visible_log(PTT_NUM, before_visible);	// �e�}�[�J�� visible ��Ԃ̋L�^
	glDisable( GL_LIGHTING );	// �����̖�����
	glDisable( GL_DEPTH_TEST );	// �A�ʏ����̖�����
}

// �����̐ݒ�
void mySetLight(void){
	GLfloat light_diffuse[]	 = { 0.9, 0.9, 0.9, 1.0 }; // �g�U���ˌ�
	GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 }; // ���ʔ��ˌ�
	GLfloat light_ambient[]  = { 0.3, 0.3, 0.3, 0.1 }; // ����
	GLfloat light_position[] = { 100.0, -200.0, 200.0, 0.0 }; // �ʒu�Ǝ��
	/* �����̐ݒ� */
	glLightfv( GL_LIGHT0, GL_DIFFUSE, light_diffuse );		// �g�U���ˌ��̐ݒ�
	glLightfv( GL_LIGHT0, GL_SPECULAR, light_specular );	// ���ʔ��ˌ��̐ݒ�
	glLightfv( GL_LIGHT0, GL_AMBIENT, light_ambient );		// �����̐ݒ�
	glLightfv( GL_LIGHT0, GL_POSITION, light_position );	// �ʒu�Ǝ�ނ̐ݒ�

	glShadeModel( GL_SMOOTH );	// �V�F�[�f�B���O�̎�ނƐݒ�
	glEnable( GL_LIGHT0 );		// �����̗L����
}

void mySetMaterial( GLfloat R, GLfloat G, GLfloat B){
	GLfloat mat_diffuse[]  = { R, G, B, 1.0 };	// �g�U���̔��ˌW��
	GLfloat mat_specular[] = { R, G, B, 1.0 };	// ���ʌ��̔��ˌW��
	GLfloat mat_ambient[]  = { R, G, B, 1.0 };	// �����̔��ˌW��
	GLfloat shininess[]    = { 50.0 };					// ���ʌ��̎w��

	/* �ގ����� */
	glMaterialfv( GL_FRONT, GL_DIFFUSE, mat_diffuse );		// �g�U���˂̐ݒ�
	glMaterialfv( GL_FRONT, GL_SPECULAR, mat_specular );	// ���ʔ��˂̐ݒ�
	glMaterialfv( GL_FRONT, GL_AMBIENT, mat_ambient );		// �����˂̐ݒ�
	glMaterialfv( GL_FRONT, GL_SHININESS, shininess );		// ���ʔ��˂̉s���̐ݒ�
}

// ���̃t���[���ł̃}�[�J�� visible�����i�[
void visible_log(int num, int *before){
	int i;

	for(i = 0; i < num; i++){
		*before = object[i].visible;
		before++;
	}
}


// �V�����}�[�J���o�������Ȃ� '1' ��Ԃ�
int myCheck_appear(int before, int now){
	if(!before){
		if(now){
			return 1;
		}
	}
	return 0;
}

// �}�[�J�������Ȃ��Ȃ����Ȃ� '1' ��Ԃ�
int myCheck_disappear(int before, int now){
	if(before){
		if(!now){
			return 1;
		}
	}
	return 0;
}

// 3D�I�u�W�F�N�g��`��(�����ɂ���ĕω�)
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

// �����ɂ���ĐF��ω�������
void mySelectColor(GLfloat R, GLfloat G, GLfloat B){
	mySetMaterial( R, G, B);
}

// double �̐�Βl��Ԃ�
double abusolute_double(double d){
	if(d < 0.0)
		return -d;
	else
		return d;
}

// 2�̃p�b�g�̋������v�Z
void mydistance(double patt1_trans[3][4], double patt2_trans[3][4], double *re_x, double *re_y, double *re_z){
	*re_x = patt1_trans[0][3] - patt2_trans[0][3];
	*re_y = patt1_trans[1][3] - patt2_trans[1][3];
	*re_z = patt1_trans[2][3] - patt2_trans[2][3];
}

// �����̐�����`��
void myLineLoop(int s, int e){
	int i;

	glBegin( GL_LINES );
		for( i=s; i < e; i++){
			glVertex3f( points[i].start_point[0], points[i].start_point[1], points[i].start_point[2] );
			glVertex3f( points[i].end_point[0], points[i].end_point[1], points[i].end_point[2] );
		}
	glEnd();
}

// �}�[�J������鐳���`��`�悷��֐�
void myMarkSquare (int patt_id){
	glBegin( GL_LINE_LOOP );
		glVertex3f( -object[patt_id].patt_width/2.0, -object[patt_id].patt_width/2.0, 0.0 );
		glVertex3f(  object[patt_id].patt_width/2.0, -object[patt_id].patt_width/2.0, 0.0 );
		glVertex3f(  object[patt_id].patt_width/2.0,  object[patt_id].patt_width/2.0, 0.0 );
		glVertex3f( -object[patt_id].patt_width/2.0,  object[patt_id].patt_width/2.0, 0.0 );
	glEnd();
}

// �J�������W����}�[�J���W�ւ̕ϊ�
void myChangeBase(int mark_id){
		double begin[3];
		int i;

		begin[0] = object[mark_id].patt_trans[0][3];
		begin[1] = object[mark_id].patt_trans[1][3];
		begin[2] = object[mark_id].patt_trans[2][3];
		
		for(i = 0; i < L_P_count; i++){
			points[i].start_point[0] = points[i].start_point[0] - begin[0];
			points[i].start_point[1] = -( points[i].start_point[1] - begin[1] ); // ���W�������]����̂�
			points[i].start_point[2] = -( points[i].start_point[2] - begin[2] ); // ���W�������]����̂�
			points[i].end_point[0]   = points[i].end_point[0] - begin[0];
			points[i].end_point[1]   = -( points[i].end_point[1] - begin[1] ); // ���W�������]����̂�
			points[i].end_point[2]   = -( points[i].end_point[2] - begin[2] ); // ���W�������]����̂�
		}
}

// �}�[�J���W����J�������W�ւ̕ϊ�
void myChangeBaseInv(int mark_id){
	double end[3];
	int i;

	end[0] = object[mark_id].patt_trans[0][3];
	end[1] = object[mark_id].patt_trans[1][3];
	end[2] = object[mark_id].patt_trans[2][3];

	for(i = 0; i < L_P_count; i++){
		points[i].start_point[0] = points[i].start_point[0] + end[0];
		points[i].start_point[1] = -points[i].start_point[1] + end[1]; // ���W�������]����̂�
		points[i].start_point[2] = -points[i].start_point[2] + end[2]; // ���W�������]����̂�
		points[i].end_point[0]   = points[i].end_point[0] + end[0];
		points[i].end_point[1]   = -points[i].end_point[1] + end[1]; // ���W�������]����̂�
		points[i].end_point[2]   = -points[i].end_point[2] + end[2]; // ���W�������]����̂�
	}
}

// �s��� s ���� d �ɃR�s�[����
void myCopyTrans (double s[3][4], double d[3][4]){
	int i, k;

	for(i=0; i < 3; i++){
		for(k = 0; k < 4; k++){
			d[i][k] = s[i][k];
		}
	}
}

// LINES_POINT �� MOVE_BALL�^�̒��g�̏����ݒ���s��
void myMoveBallInit(LINES_POINT *p){
	int i;
	
	for( i = 0; i < 3; i++){
		p->ball.next_sphere_xyz[i] = p->start_point[i];
		p->ball.movement[i] = ( p->start_point[i] - p->end_point[i] ) / MOVE_BALL_FRAME;
	}
}

// �ړ����鋅�̃A�j���[�V������`��
void myMoveBall(LINES_POINT *p){
	int i;

	mySelectColor( 0.0, 1.0, 1.0); // �ގ������̐ݒ�
	
	glPushMatrix();
		glTranslatef( p->ball.next_sphere_xyz[0], p->ball.next_sphere_xyz[1], p->ball.next_sphere_xyz[2] );
		glutSolidSphere( 5.0, 10, 10 );
	glPopMatrix();
	
	/* ���̋��̍��W���X�V */
	for( i = 0; i < 3; i++){
		p->ball.next_sphere_xyz[i] -= p->ball.movement[i];
	}

	/* �����ǂꂩ���I�[�_�܂ł������̂Ȃ���W���X�^�[�g�n�_�Ƀ��Z�b�g */
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