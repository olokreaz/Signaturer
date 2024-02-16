#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using u64 = std::uint64_t;
using u8 = std::uint8_t;
using std::format;
namespace fs = std::filesystem;

/*
Приложение для Windows.
1.	Предложить свой алгоритм формирования цифровой подписи для данных .
2.	Подпись должна быть индивидуальна  с достаточно высокой вероятностью. Как минимум возможность формирования 1024 варианта подписи.

----------------------------------------------------------------------------------
1.	Приложение читает данные из файла, указанного при запуске или запрошенного от пользователя .
2.	Проверяет наличие цифровой подписи этого файла данных.
3.	О наличии/ отсутствии подписи сообщает.
4.	Предлагает убрать/добавить подпись.
5.	Результат писать в другой файл. Имя его получать при запуске  или формировать автоматически.
*/

std::string padData( const std::string &data )
{
	std::string paddedData = data;
	while ( paddedData . size( ) % 8 != 0 ) paddedData += '\0';
	return paddedData;
}

// Произвольная операция обработки блока данных
u64 processBlock( const std::string &block, size_t blockIndex )
{
	// Преобразуем блок данных в uint64_t
	u64 blockHash = *( u64 * ) block . data( );
	// Выполняем некоторое сложное преобразование хэша
	return blockHash ^ ( blockIndex * 1024 ); // Пример преобразования: XOR с умноженным индексом
}

// Простой хэш-алгоритм для блоков по 8 байт
u64 simpleHash64( const std::string &data )
{
	std::string paddedData = padData( data );
	u64         hash       = 0;
	// Обрабатываем каждый блок данных
	for ( size_t i = 0 ; i < paddedData . size( ) ; i += 8 )
	{
		std::string block = paddedData . substr( i, 8 );
		// Обновляем хэш, используя обработанный блок данных и индекс блока
		hash += processBlock( block, i / 8 );
	}
	return hash;
}

std::vector < char > readFileBytes( const fs::path &filename )
{
	std::ifstream file( filename, std::ios::binary );

	if ( !file . is_open( ) )
	{
		std::cerr << "Не удалось открыть файл: " << filename << std::endl;
		exit( 1 );
	}

	auto filesize = file_size( filename );
	// Создаем буфер для хранения данных файла
	std::vector < char > buffer( filesize );
	// Считываем данные из файла в буфер
	file . read( buffer . data( ), filesize );

	return buffer;
}


enum class EStatusSigned
{
	kSigned ,
	kUnsigned ,
	kChangedData ,
};


constexpr auto kOffsetNotIndexSpecific  = sizeof( u64 ) + 1;
constexpr auto kOffsetWithIndexSpecific = sizeof( u64 ) + 2;

EStatusSigned checkHashInFile( std::span < u8 > dataFile )
{
	// Считываем данные из файла

	const auto data = dataFile . subspan( 0, dataFile . size( ) - kOffsetNotIndexSpecific );
	const auto hash = dataFile . subspan( dataFile . size( ) - kOffsetWithIndexSpecific, sizeof( u64 ) );

	// Проверяем, достаточно ли данных в файле

	if ( data . size( ) < 9 ) return EStatusSigned::kUnsigned;

	const u8 *pHashData = data . data( ) + data . size( ) - sizeof( u64 ) - 1;
	const u8 *pFlag     = data . data( ) + data . size( ) - 1;

	// Проверяем, достаточно ли данных в файлеff(-10) bc(-9) ac(-8) dc(-7) ac(-6) bc(-5) ac(-4) dc(-3) bc(-2) ff(-1)

	// Проверяем, чтобы первый и последний байты были равны 0xFF
	if ( u8 ff = 0xff ; *pFlag != ff ) return EStatusSigned::kUnsigned;

	// Проверяем, чтобы значение хеша совпадало с ожидаемым
	u64 actualHash;

	std::memcpy( &actualHash, pHashData, sizeof( u64 ) );

	u64 expectedHash = simpleHash64( { data . data( ), data . data( ) + data . size( ) } );

	if ( actualHash == expectedHash ) return EStatusSigned::kSigned;

	return EStatusSigned::kChangedData;
}

int main( int argc, char **argv )
{

	fs::path pathInput;
	fs::path pathOutput;


	switch ( argc )
	{
		case 1 :
		{
			std::cout << "input path to file:";
			std::string path;
			std::cin >> path;
			pathInput  = path;
			pathOutput = pathInput . replace_extension( ".signed" );
			break;
		}
		case 2 :
		{
			pathInput  = argv[ 1 ];
			pathOutput = pathInput . replace_extension( ".signed" );
		}
		case 3 :
		{
			pathInput  = argv[ 1 ];
			pathOutput = argv[ 2 ];
		}
		default :
		{
			std::cerr << "Invalid arguments, app.exe [input] [output]" << std::endl;
			return 1;
		}
	}


	const auto dataFile = readFileBytes( pathInput );
	const auto data     = std::span( dataFile . data( ), dataFile . size( ) - dataFile[ dataFile . size( ) - 1 ] == 0xff ? kOffsetWithIndexSpecific : 0 );

	auto status = checkHashInFile( { ( uint8_t * ) dataFile . data( ), dataFile . size( ) } );

	switch ( status )
	{
		case EStatusSigned::kSigned : std::cout << "File is signed" << std::endl;
			break;
		case EStatusSigned::kUnsigned : std::cout << "File is unsigned" << std::endl;
			break;
		case EStatusSigned::kChangedData : std::cout << "File is maybe changed" << std::endl;
			break;
	}

	int input_action;

	do
	{
		std::cout << "1. Add sign\n";
		std::cout << "2. Remove sign\n";
		std::cout << "3. Resign\n";
		std::cout << "4. Exit\n";

		std::cout << "Select action: ";
		std::cin >> input_action;
	} while ( input_action < 1 || input_action > 4 );

	switch ( input_action )
	{
		case 1 :
		{
			std::ofstream file( pathOutput, std::ios::binary | std::ios::app );
			if ( !file . is_open( ) )
			{
				std::cerr << "Can't opened file " << pathOutput << std::endl;
				exit( 1 );
			}

			// Записываем данные в файл
			//					file.write(data.data(), data.size());
			// Вычисляем хэш для данных
			u64 hash = simpleHash64( { data . data( ), data . data( ) + data . size( ) } );
			// Записываем хэш в файл
			file . write( static_cast < char * >( &hash ), sizeof hash );
			// Записываем флаг в файл
			file . write( "\xFF", 1 );
			break;
		}
		case 2 :
		{
			std::ofstream file( pathOutput, std::ios::binary | std::ios::trunc );
			if ( !file . is_open( ) )
			{
				std::cerr << "Can't opened file " << pathOutput << std::endl;
				exit( 1 );
			}
			// Записываем данные в файл
			file . write( data . data( ), data . size( ) );
			break;
		}
		case 3 :
		{
			std::ofstream file( pathOutput, std::ios::binary | std::ios::trunc );
			if ( !file . is_open( ) )
			{
				std::cerr << "Can't opened file " << pathOutput << std::endl;
				exit( 1 );
			}
			// Записываем данные в файл
			file . write( data . data( ), data . size( ) );
			// Вычисляем хэш для данных
			u64 hash = simpleHash64( { data . data( ), data . data( ) + data . size( ) } );
			// Записываем хэш в файл
			file . write( static_cast < char * >( &hash ), sizeof hash );
			// Записываем флаг в файл
			file . write( "\xFF", 1 );
			break;
		}
		case 4 : { return 0; }
	}

	return 0;
}