#ifdef WINDOWS
#include <windows.h>
#endif

#include <algorithm>
#include <vector>
#include <iostream>

#include <GL/gl.h>
#include <GL/glut.h>

#include <Eigen/Dense>
#include <Eigen/Geometry>

using namespace Eigen;

////////////////////////////////////////////////////////////////////////////////
// constants
const unsigned int window_width = 512;
const unsigned int window_height = 512;

float g_theta(0);
float g_phi(0);
float g_r(5);

// GL functionality
bool initGL(int *argc, char** argv);

// rendering callbacks
void display();
void keyboard(unsigned char key, int x, int y);
void mouse(int button, int state, int x, int y);
void motion(int x, int y);

int mouse_old_x, mouse_old_y;
int mouse_buttons = 0;

struct ParticleData
{
	std::vector< Vector3f > particleX;
	std::vector< Vector3f > particleV;
	std::vector< float > particleM;

	std::vector< Matrix3f > particleF;
	std::vector< Matrix3f > particleR;
	std::vector< Matrix3f > particleS;
	std::vector< Matrix3f > particleFinvTrans;
	std::vector< float > particleJ;

	std::vector< float > particleVolumes;
	std::vector< float > particleDensities;
};

ParticleData g_particles;

int g_time(0);

#define GRID_H 0.25
#define TIME_STEP 0.01
#define INITIALDENSITY 400

#define YOUNGSMODULUS 1.4e5
#define POISSONRATIO 0.2

#define MU ( YOUNGSMODULUS / ( 2 * ( 1 + POISSONRATIO ) ) )
#define LAMBDA ( YOUNGSMODULUS * POISSONRATIO / ( ( 1 + POISSONRATIO ) * ( 1 - 2 * POISSONRATIO ) ) )
#define BETA 1

#define DECLARE_WEIGHTARRAY( NAME ) float buf_##NAME[12]; float * NAME[] = { &buf_##NAME[1], &buf_##NAME[5], &buf_##NAME[9] };

float matrixDoubleDot( const Matrix3d& a, const Matrix3d& b )
{
	return
		a(0,0) * b(0,0) + a(0,1) * b(0,1) + a(0,2) * b(0,2) + 
		a(1,0) * b(1,0) + a(1,1) * b(1,1) + a(1,2) * b(1,2) + 
		a(2,0) * b(2,0) + a(2,1) * b(2,1) + a(2,2) * b(2,2);
}


////////////////////////////////////////////////////////////////////////////////
// Program main
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	
	// test code for force differential:
	Vector3d w(1,1,1);
	w.normalize();
	Matrix3d Rorig = AngleAxisd( 0.25*M_PI, w ).toRotationMatrix();
	Matrix3d Sorig = Matrix3d::Zero();
	Sorig(0,0) = 1;
	Sorig(1,1) = 2;
	Sorig(2,2) = 3;

	Matrix3d F = Matrix3d::Random();//Rorig * Sorig;
	Matrix3d R;
	Matrix3d S;
	Affine3d trans( F );	
	trans.computeRotationScaling( &R, &S );
	Matrix3d Finv;
	Matrix3d FinvTrans;
	double J;
	bool invertible;
	
	F.computeInverseAndDetWithCheck( Finv, J, invertible );
	FinvTrans = Finv.transpose();
	
	Matrix3d dEdF = 2 * MU * ( F - R ) + LAMBDA * ( J - 1 ) * J * FinvTrans;
	
	Matrix3d dF = Matrix3d::Random() * 0.0001;
	Matrix3d F_= F + dF;
	Matrix3d R_;
	Matrix3d S_;
	Affine3d trans2( F_ );	
	trans2.computeRotationScaling( &R_, &S_ );
	Matrix3d Finv_;
	Matrix3d FinvTrans_;
	double J_;
	
	F_.computeInverseAndDetWithCheck( Finv_, J_, invertible );
	FinvTrans_ = Finv_.transpose();
	
	Matrix3d dEdF_ = 2 * MU * ( F_ - R_ ) + LAMBDA * ( J_ - 1 ) * J_ * FinvTrans_;
	
	
	double dJ = J * matrixDoubleDot( FinvTrans, dF );
	
	
	Matrix3d M = R.transpose() * dF - dF.transpose() * R;
	
	Matrix3d G;
	G(0,0) = S(0,0) + S(1,1);
	G(1,1) = S(0,0) + S(2,2);
	G(2,2) = S(1,1) + S(2,2);
	
	G(0,1) = G(1,0) = S(1,2);
	G(0,2) = G(2,0) = -S(0,2);
	G(1,2) = G(2,1) = S(0,1);
	
	Vector3d m( M(0,1), M(0,2), M(1,2) );
	
	w = G.inverse() * m;
	
	Matrix3d RtdR;
	RtdR(0,0) = RtdR(1,1) = RtdR(2,2) = 0;
	
	RtdR(0,1) = w[0];
	RtdR(1,0) = -w[0];
	
	RtdR(0,2) = w[1];
	RtdR(2,0) = -w[1];
	
	RtdR(1,2) = w[2];
	RtdR(2,1) = -w[2];
	
	
	Matrix3d dR = R * RtdR;
	
	Matrix3d dFinvTrans = - FinvTrans * dF.transpose() * FinvTrans;
	
	Matrix3d Ap = 2 * MU * ( dF - dR ) + LAMBDA * ( dJ * J * FinvTrans + ( J - 1 ) * ( dJ * FinvTrans + J * dFinvTrans ) );
	
	std::cerr << dEdF_ - dEdF << std::endl << std::endl;
	std::cerr << Ap << std::endl << std::endl;
	
	
	
	
	
	// initial configuration:
	Vector3f rotVector( 1, 9, 3 );
	rotVector.normalize();
	float particleSpacing( 0.1 );
	float particleVolume = particleSpacing * particleSpacing * particleSpacing;

	for( int i=3; i <= 7; ++i )
	{
		for( int j=3; j <= 7; ++j )
		{
			for( int k=3; k <= 7; ++k )
			{
				Vector3f pos( 0.1 * float( i ) - 0.5, 0.1 * float( j ) - 0.5, 0.1 * float( k ) - 0.5 );
				
				g_particles.particleX.push_back( pos );
				g_particles.particleV.push_back( rotVector.cross( pos ) + 2 * rotVector.dot( pos ) * rotVector );
				g_particles.particleM.push_back( INITIALDENSITY * particleVolume );

				g_particles.particleF.push_back( Matrix3f::Identity() );
				g_particles.particleR.push_back( Matrix3f::Identity() );
				g_particles.particleS.push_back( Matrix3f::Identity() );
				g_particles.particleFinvTrans.push_back( Matrix3f::Identity() );
				g_particles.particleJ.push_back( 1.0f );

			}
		}
	}

	srand( 10 );
	initGL( &argc, argv );
	
	// start rendering mainloop
	glutMainLoop();

}

////////////////////////////////////////////////////////////////////////////////
//! Initialize GL
////////////////////////////////////////////////////////////////////////////////
bool initGL(int *argc, char **argv)
{
    glutInit(argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Thnowing on our roatht!");
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
	glutMouseFunc(mouse);
    glutMotionFunc(motion);
	
    // default initialization
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glDisable(GL_DEPTH_TEST);
	glEnable( GL_CULL_FACE );

    // viewport
    glViewport(0, 0, window_width, window_height);

    // projection
    glMatrixMode(GL_PROJECTION);

    glLoadIdentity();
	gluPerspective(	30,
 					double( window_width ) / window_height,
 					0.01,
 					20);
	
    return true;
}


class Grid
{
public:
	Grid( const ParticleData& d )
	{
		// work out the physical size of the grid:
		m_xmin = m_ymin = m_zmin = 1.e10;
		m_xmax = m_ymax = m_zmax = -1.e10;

		for( size_t i=0; i < d.particleX.size(); ++i )
		{
			minMax( d.particleX[i][0], m_xmin, m_xmax );
			minMax( d.particleX[i][1], m_ymin, m_ymax );
			minMax( d.particleX[i][2], m_zmin, m_zmax );
		}
		
		// calculate grid dimensions and quantize bounding box:
		m_nx = fixDim( m_xmin, m_xmax );
		m_ny = fixDim( m_ymin, m_ymax );
		m_nz = fixDim( m_zmin, m_zmax );
				
		// little array with indexes going from -1 to store shape function weights
		// on each dimension:
		DECLARE_WEIGHTARRAY( w );
		Vector3i particleCell;
		
		// calculate masses:
		m_gridMasses.resize( m_nx * m_ny * m_nz );
		m_gridMasses.setZero();

		for( size_t p = 0; p < d.particleX.size(); ++p )
		{
			cellAndWeights( d.particleX[p], particleCell, w );
			// splat the masses:
			for( int i=-1; i < 3; ++i )
			{
				for( int j=-1; j < 3; ++j )
				{
					for( int k=-1; k < 3; ++k )
					{
						int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
						float weight = w[0][i] * w[1][j] * w[2][k];
						m_gridMasses[ idx ] +=
							d.particleM[p] * weight;
					}
				}
			}
		}

		// calculate velocities:
		m_gridVelocities.resize( m_nx * m_ny * m_nz * 3 );
		m_gridVelocities.setZero();

		for( size_t p = 0; p < d.particleX.size(); ++p )
		{
			cellAndWeights( d.particleX[p], particleCell, w );
			
			// splat the velocities:
			for( int i=-1; i < 3; ++i )
			{
				for( int j=-1; j < 3; ++j )
				{
					for( int k=-1; k < 3; ++k )
					{
						int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
						if( m_gridMasses[idx] > 0 )
						{
							float particleMass = d.particleM[p];
							float gridCellMass = m_gridMasses[idx];
							float overallWeight = w[0][i] * w[1][j] * w[2][k] *
								( particleMass / gridCellMass );

							m_gridVelocities.segment<3>( 3 * idx ) += overallWeight * d.particleV[p];
						}
					}
				}
			}
		}		
	}

	void draw()
	{
		glColor3f( 0,0.3,0 );
		glBegin( GL_LINES );

		// xy
		for( int i=0; i <= m_nx; ++i )
		{
			for( int j=0; j <= m_ny; ++j )
			{
				glVertex3f( m_xmin + i * GRID_H, m_ymin + j * GRID_H, m_zmin );
				glVertex3f( m_xmin + i * GRID_H, m_ymin + j * GRID_H, m_zmax );
			}
		}
		// zy
		for( int i=0; i <= m_nz; ++i )
		{
			for( int j=0; j <= m_ny; ++j )
			{
				glVertex3f( m_xmin, m_ymin + j * GRID_H, m_zmin + i * GRID_H );
				glVertex3f( m_xmax, m_ymin + j * GRID_H, m_zmin + i * GRID_H );
			}
		}

		// xz
		for( int i=0; i <= m_nx; ++i )
		{
			for( int j=0; j <= m_nz; ++j )
			{
				glVertex3f( m_xmin + i * GRID_H, m_ymin, m_zmin + j * GRID_H );
				glVertex3f( m_xmin + i * GRID_H, m_ymax, m_zmin + j * GRID_H );
			}
		}
		glEnd();
	}
	
	void computeDensities( ParticleData& d )
	{
		d.particleDensities.resize( d.particleX.size(), 0 );
		
		// little array with indexes going from -1 to store shape function weights
		// on each dimension:
		DECLARE_WEIGHTARRAY( w );
		Vector3i particleCell;

		for( size_t p = 0; p < d.particleX.size(); ++p )
		{
			cellAndWeights( d.particleX[p], particleCell, w );
			
			// transfer densities back onto the particles:
			for( int i=-1; i < 3; ++i )
			{
				for( int j=-1; j < 3; ++j )
				{
					for( int k=-1; k < 3; ++k )
					{
						int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
						
						// accumulate the particle's density:
						d.particleDensities[p] += w[0][i] * w[1][j] * w[2][k] * m_gridMasses[ idx ] / ( GRID_H * GRID_H * GRID_H );
						
					}
				}
			}
		}
	}

	void updateParticleVelocities( ParticleData& d )
	{
		// little array with indexes going from -1 to store shape function weights
		// on each dimension:
		DECLARE_WEIGHTARRAY( w );
		Vector3i particleCell;

		// this is, like, totally doing things FLIP style. The paper recommends a combination of FLIP and PIC...
		for( size_t p = 0; p < d.particleX.size(); ++p )
		{
			cellAndWeights( d.particleX[p], particleCell, w );

			for( int i=-1; i < 3; ++i )
			{
				for( int j=-1; j < 3; ++j )
				{
					for( int k=-1; k < 3; ++k )
					{
						int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
						d.particleV[p] += w[0][i] * w[1][j] * w[2][k] *
							( m_gridVelocities.segment<3>( 3 * idx ) - m_prevGridVelocities.segment<3>( 3 * idx ) );
					}
				}
			}
		}
	}
	
	float matrixDoubleDot( const Matrix3f& a, const Matrix3f& b )
	{
		return
			a(0,0) * b(0,0) + a(0,1) * b(0,1) + a(0,2) * b(0,2) + 
			a(1,0) * b(1,0) + a(1,1) * b(1,1) + a(1,2) * b(1,2) + 
			a(2,0) * b(2,0) + a(2,1) * b(2,1) + a(2,2) * b(2,2);
	}

	void applyImplicitUpdateMatrix( const ParticleData& d, const VectorXf& v, VectorXf& result )
	{
		result = v;

		// little array with indexes going from -1 to store shape function derivative weights
		// on each dimension:
		DECLARE_WEIGHTARRAY( w );
		DECLARE_WEIGHTARRAY( dw );
		Vector3i particleCell;
		
		for( size_t p = 0; p < d.particleF.size(); ++p )
		{
			cellAndWeights( d.particleX[p], particleCell, w, dw );
			
			// work out deformation gradient differential for this particle when grid nodes are
			// moved by their respective v * Dt
			Matrix3f dFp = Matrix3f::Zero();
			for( int i=-1; i < 3; ++i )
			{
				for( int j=-1; j < 3; ++j )
				{
					for( int k=-1; k < 3; ++k )
					{
						int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
						Vector3f weightGrad( dw[0][i] * w[1][j] * w[2][k], w[0][i] * dw[1][j] * w[2][k], w[0][i] * w[1][j] * dw[2][k] );
						Vector3f dx = v.segment<3>( 3 * idx ) * TIME_STEP;
						dFp += dx * weightGrad.transpose() * d.particleF[p];
					}
				}
			}
			
			// work out energy derivatives with respect to the deformation gradient at this particle:
			// Ap = d2Psi / dF dF : dF (see the tech report). We've got dF, so just plug that into the
			// formulae...
			
			// if you look down in updateGridVelocities, you'll find this expression, which is used while
			// computing the force on a grid node:
			
			// 2 * MU * ( d.particleF[p] - d.particleR[p] ) + LAMBDA * ( d.particleJ[p] - 1 ) * d.particleJ[p] * d.particleFinvTrans[p];
			
			// what we're doing here is just assuming dFp is small and working out the corresponding variation in
			// that expression...
			
			float J = d.particleJ[p];

			// work out a couple of basic differentials:
			float dJ = J * matrixDoubleDot( d.particleFinvTrans[p], dFp );
			Matrix3f dFInvTrans = - d.particleFinvTrans[p] * dFp.transpose() * d.particleFinvTrans[p];
			
			// fiddy calculation for dR:
			Matrix3f M = d.particleR[p].transpose() * dFp - dFp.transpose() * d.particleR[p];
			
			Matrix3f G;
			G(0,0) = d.particleS[p](0,0) + d.particleS[p](1,1);
			G(1,1) = d.particleS[p](0,0) + d.particleS[p](2,2);
			G(2,2) = d.particleS[p](1,1) + d.particleS[p](2,2);
			
			G(0,1) = G(1,0) = d.particleS[p](1,2);
			G(0,2) = G(2,0) = -d.particleS[p](0,2);
			G(1,2) = G(2,1) = d.particleS[p](0,1);
			
			Vector3f components = G.inverse() * Vector3f( M(0,1), M(0,2), M(1,2) );
			
			Matrix3f RtdR;
			RtdR(0,0) = RtdR(1,1) = RtdR(2,2) = 0;

			RtdR(0,1) = components[0];
			RtdR(1,0) = -components[0];

			RtdR(0,2) = components[1];
			RtdR(2,0) = -components[1];

			RtdR(1,2) = components[2];
			RtdR(2,1) = -components[2];
			
			Matrix3f dR = d.particleR[p] * RtdR;
			
			// start with differential of 2 * MU * ( F - R )...
			Matrix3f Ap = 2 * MU * ( dFp - dR );
			
			// add on differential of LAMBDA * ( J - 1 ) * J * F^-t
			// = LAMBDA * ( d( J - 1 ) * J F^-T + ( J - 1 ) * d( J F^-t ) )
			// = LAMBDA * ( dJ * J F^-T + ( J - 1 ) * ( dJ F^-t + J * d( F^-t ) )
			Ap += LAMBDA * ( dJ * J * d.particleFinvTrans[p] + ( J - 1 ) * ( dJ * d.particleFinvTrans[p] + J * dFInvTrans ) );
			
			Matrix3f forceMatrix = d.particleVolumes[p] * Ap * d.particleF[p].transpose();

			for( int i=-1; i < 3; ++i )
			{
				for( int j=-1; j < 3; ++j )
				{
					for( int k=-1; k < 3; ++k )
					{
						int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
						Vector3f weightGrad( dw[0][i] * w[1][j] * w[2][k], w[0][i] * dw[1][j] * w[2][k], w[0][i] * w[1][j] * dw[2][k] );
											
						// work out force on this node due to this particle:
						Vector3f df = forceMatrix * weightGrad;
						
						// add on difference in velocity due to this force:
						result.segment<3>( 3 * idx ) += BETA * TIME_STEP * df;
					}
				}
			}
		}
	}

	// stabilised biconjugate gradient solver copy pasted out of Eigen
	bool bicgstab(
		const ParticleData& d,
		const VectorXf& rhs,
		VectorXf& x,
		int& iters,
		float& tol_error )
	{
		using std::sqrt;
		using std::abs;
		typedef float RealScalar;
		typedef float Scalar;
		typedef Matrix<Scalar,Dynamic,1> VectorType;
		RealScalar tol = tol_error;
		int maxIters = iters;

		int n = rhs.size();
		VectorType r;
		applyImplicitUpdateMatrix( d, x, r );
		r = rhs - r;
		VectorType r0 = r;

		RealScalar r0_sqnorm = r0.squaredNorm();
		RealScalar rhs_sqnorm = rhs.squaredNorm();
		if(rhs_sqnorm == 0)
		{
			x.setZero();
			return true;
		}
		Scalar rho    = 1;
		Scalar alpha  = 1;
		Scalar w      = 1;

		VectorType v = VectorType::Zero(n), p = VectorType::Zero(n);
		VectorType kt(n), ks(n);

		VectorType s(n), t(n);

		RealScalar tol2 = tol*tol;
		int i = 0;
		int restarts = 0;

		while ( r.squaredNorm()/rhs_sqnorm > tol2 && i<maxIters )
		{
			Scalar rho_old = rho;

			rho = r0.dot(r);
			if (internal::isMuchSmallerThan(rho,r0_sqnorm))
			{
				// The new residual vector became too orthogonal to the arbitrarily choosen direction r0
				// Let's restart with a new r0:
				r0 = r;
				rho = r0_sqnorm = r.squaredNorm();
				if(restarts++ == 0)
				{
					i = 0;
				}
			}
			Scalar beta = (rho/rho_old) * (alpha / w);
			p = r + beta * (p - w * v);

			applyImplicitUpdateMatrix( d, p, v );

			alpha = rho / r0.dot(v);
			s = r - alpha * v;
			
			applyImplicitUpdateMatrix( d, s, t );

			RealScalar tmp = t.squaredNorm();
			if(tmp>RealScalar(0))
			{
				w = t.dot(s) / tmp;
			}
			else
			{
				w = Scalar(0);
			}
			x += alpha * p + w * s;
			r = s - w * t;
			++i;
		}
		tol_error = sqrt(r.squaredNorm()/rhs_sqnorm);
		iters = i;
		return true; 
	}
	

	void updateGridVelocities( const ParticleData& d )
	{
		m_prevGridVelocities = m_gridVelocities;

		// little array with indexes going from -1 to store shape function derivative weights
		// on each dimension:
		DECLARE_WEIGHTARRAY( w );
		DECLARE_WEIGHTARRAY( dw );
		Vector3i particleCell;

		// work out forces on grid points:
		VectorXf forces( m_gridVelocities.size() );
		forces.setZero();
		for( size_t p = 0; p < d.particleX.size(); ++p )
		{
			cellAndWeights( d.particleX[p], particleCell, w, dw );
			
			Matrix3f dEdF = 2 * MU * ( d.particleF[p] - d.particleR[p] ) + LAMBDA * ( d.particleJ[p] - 1 ) * d.particleJ[p] * d.particleFinvTrans[p];
			
			for( int i=-1; i < 3; ++i )
			{
				for( int j=-1; j < 3; ++j )
				{
					for( int k=-1; k < 3; ++k )
					{
						int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
						Vector3f weightGrad( dw[0][i] * w[1][j] * w[2][k], w[0][i] * dw[1][j] * w[2][k], w[0][i] * w[1][j] * dw[2][k] );
						forces.segment<3>( 3 * idx ) -= d.particleVolumes[p] * dEdF * d.particleF[p].transpose() * weightGrad;
					}
				}
			}
		}
		
		// work out forward velocity update - that's equation 10 in the paper:
		VectorXf forwardVelocities( m_gridVelocities.size() );
		for( int i=0; i < m_gridMasses.size(); ++i )
		{
			if( m_gridMasses[i] == 0 )
			{
				forwardVelocities.segment<3>( 3 * i ) = m_gridVelocities.segment<3>( 3 * i );
			}
			else
			{
				Vector3f force = forces.segment<3>( 3 * i );
				Vector3f velocity = m_gridVelocities.segment<3>( 3 * i );
				forwardVelocities.segment<3>( 3 * i ) = velocity + TIME_STEP * force / m_gridMasses[i];
			}
		}
		
		float tol_error = 1.e-7;
		int iters = 30;
		bicgstab(
				d,
				forwardVelocities,
				m_gridVelocities,
				iters,
				tol_error );
		
		
	}

	
	void updateDeformationGradients( ParticleData& d )
	{
		// little array with indexes going from -1 to store shape function derivative weights
		// on each dimension:
		DECLARE_WEIGHTARRAY( w );
		DECLARE_WEIGHTARRAY( dw );
		Vector3i particleCell;
		
		for( size_t p = 0; p < d.particleX.size(); ++p )
		{
			cellAndWeights( d.particleX[p], particleCell, w, dw );
			
			Vector3f v = Vector3f::Zero();
			Matrix3f delV = Matrix3f::Zero();
			for( int i=-1; i < 3; ++i )
			{
				for( int j=-1; j < 3; ++j )
				{
					for( int k=-1; k < 3; ++k )
					{
						int idx = coordsToIndex( particleCell[0] + i, particleCell[1] + j, particleCell[2] + k );
						Vector3f weightGrad( dw[0][i] * w[1][j] * w[2][k], w[0][i] * dw[1][j] * w[2][k], w[0][i] * w[1][j] * dw[2][k] );
						Vector3f vSample = m_gridVelocities.segment<3>( 3 * idx );
						delV += vSample * weightGrad.transpose();
						v += vSample * w[0][i] * w[1][j] * w[2][k];
					}
				}
			}
			Matrix3f newParticleF = ( Matrix3f::Identity() + TIME_STEP * delV ) * d.particleF[p];
			d.particleF[p] = newParticleF;

			// find determinant and inverse transpose of deformation gradient:
			bool invertible;
			d.particleF[p].computeInverseAndDetWithCheck( d.particleFinvTrans[p], d.particleJ[p], invertible );
			if( invertible )
			{
				d.particleFinvTrans[p].transposeInPlace();

				// find polar decomposition of deformation gradient:
				Affine3f trans( d.particleF[p] );	
				trans.computeRotationScaling( &d.particleR[p], &d.particleS[p] );
			}
		}
	}
	

	static inline float N( float x )
	{
		float ax = fabs(x);
		if( ax < 1 )
		{
			return 0.5f * ax * ax * ax - ax * ax + 2.0f/3;
		}
		else if( ax < 2 )
		{
			return -ax * ax * ax / 6 + ax * ax - 2 * ax + 4.0f/3;
		}
		else
		{
			return 0;
		}
	}

	static inline float DN( float x )
	{
		if( x < 0 )
		{
			return -DN( -x );
		}
		
		if( x < 1 )
		{
			return x * ( 1.5 * x - 2 );
		}
		else if( x < 2 )
		{
			x -= 2;
			return -0.5 * x * x;
		}
		else
		{
			return 0;
		}
	}

	inline int coordsToIndex( int x, int y, int z )
	{
		return x + m_nx * ( y + z * m_ny );
	}

	void cellAndWeights( const Vector3f& particleX, Vector3i& particleCell, float *w[], float** dw = 0 )
	{
		Vector3f positionInCell;
		positionInCell[0] = ( particleX[0] - m_xmin ) / GRID_H;
		positionInCell[1] = ( particleX[1] - m_xmin ) / GRID_H;
		positionInCell[2] = ( particleX[2] - m_xmin ) / GRID_H;
		
		particleCell[0] = (int)floor( positionInCell[0] );
		particleCell[1] = (int)floor( positionInCell[1] );
		particleCell[2] = (int)floor( positionInCell[2] );
		
		positionInCell -= particleCell.cast<float>();
		if( dw )
		{
			for( int i=0; i < 3; ++i )
			{
				dw[i][-1] = DN( positionInCell[i] + 1 ) / GRID_H;
				dw[i][0] = DN( positionInCell[i] ) / GRID_H;
				dw[i][1] = DN( positionInCell[i] - 1 ) / GRID_H;
				dw[i][2] = DN( positionInCell[i] - 2 ) / GRID_H;
			}
		}
		
		for( int i=0; i < 3; ++i )
		{
			w[i][-1] = N( positionInCell[i] + 1 );
			w[i][0] = N( positionInCell[i] );
			w[i][1] = N( positionInCell[i] - 1 );
			w[i][2] = N( positionInCell[i] - 2 );
		}
		
	}

	inline void minMax( float x, float& min, float& max )
	{
		if( x < min )
		{
			min = x;
		}
		if( x > max )
		{
			max = x;
		}
	}

	inline int fixDim( float& min, float& max )
	{
		int n = int( ceil( ( max - min ) / GRID_H ) ) + 6;
		float padding = 0.5 * ( n * GRID_H - ( max - min ) );
		min -= padding;
		max = min + n * GRID_H;
		return n;
	}

private:

	float m_xmin;
	float m_ymin;
	float m_zmin;

	float m_xmax;
	float m_ymax;
	float m_zmax;
	
	int m_nx;
	int m_ny;
	int m_nz;

	VectorXf m_gridMasses;
	VectorXf m_gridVelocities;
	VectorXf m_prevGridVelocities;

};


////////////////////////////////////////////////////////////////////////////////
//! Display callback
////////////////////////////////////////////////////////////////////////////////
void display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(	g_r * cos( g_theta ) * sin( g_phi ),
				g_r * sin( g_theta ),
 				g_r * cos( g_theta ) * cos( g_phi ),
 				0.0, 0.0, 0.0,
 				0.0, 1.0, 0.0
	);

	// display!
	glPointSize(2);
	glBegin( GL_POINTS );
	glColor3f( 1,1,1 );
	for( size_t i = 0; i < g_particles.particleX.size(); ++i )
	{
		glVertex3f( g_particles.particleX[i][0], g_particles.particleX[i][1], g_particles.particleX[i][2] );
	}

	glEnd();
	

	// instantiate grid and rasterize!
	Grid g( g_particles );
	g.draw();
	if( g_particles.particleVolumes.empty() )
	{
		g.computeDensities( g_particles );
		g_particles.particleVolumes.resize( g_particles.particleX.size() );
		for( size_t i = 0; i < g_particles.particleDensities.size(); ++i )
		{
			g_particles.particleVolumes[i] = g_particles.particleM[i] / g_particles.particleDensities[i];
		}
	}
	
	// update grid velocities using internal stresses...
	g.updateGridVelocities( g_particles );
	
	// transfer the grid velocities back onto the particles:
	g.updateParticleVelocities( g_particles );

	// update particle deformation gradients:
	g.updateDeformationGradients( g_particles );

	// update positions...
	for( size_t i = 0; i < g_particles.particleX.size(); ++i )
	{
		g_particles.particleX[i] += g_particles.particleV[i] * TIME_STEP;
	}
	++g_time;
    glutSwapBuffers();
    glutPostRedisplay();
}


////////////////////////////////////////////////////////////////////////////////
//! Keyboard events handler
////////////////////////////////////////////////////////////////////////////////
void keyboard(unsigned char key, int /*x*/, int /*y*/)
{
	std::cerr << key << std::endl;
    switch(key)
	{
    case(27) :
        exit(0);
        break;
	case 'p':
		break;
    }
}

////////////////////////////////////////////////////////////////////////////////
//! Mouse event handlers
////////////////////////////////////////////////////////////////////////////////
void mouse(int button, int state, int x, int y)
{
    if (state == GLUT_DOWN)
	{
        mouse_buttons |= 1<<button;
    }
	else if (state == GLUT_UP)
	{
        mouse_buttons &= ~( 1<<button );
    }

    mouse_old_x = x;
    mouse_old_y = y;
    glutPostRedisplay();
}

void motion(int x, int y)
{
	float dx, dy;
	dx = x - mouse_old_x;
	dy = y - mouse_old_y;

	if (mouse_buttons & 1)
	{
		g_theta += dy * 0.2 * M_PI / 180.0;
		g_phi -= dx * 0.2 * M_PI / 180.0;
	}
	else if (mouse_buttons & 4)
	{
		g_r += dy * 0.01;
	}

	mouse_old_x = x;
	mouse_old_y = y;
}
