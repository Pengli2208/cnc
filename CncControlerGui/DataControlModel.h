#ifndef DATA_CONTROL_MODEL_H
#define DATA_CONTROL_MODEL_H

#include <wx/variant.h>
#include <wx/vector.h>
#include "CncArduino.h"

typedef wxVector<wxVector<wxVariant>> DcmItemList;
typedef wxVector<wxVariant> DcmRow;

class DataControlModel {

	public:
		DataControlModel() {}
		~DataControlModel() {}
		
		////////////////////////////////////////////////////////////////////////
		static void addKeyCheckValueRow(DcmItemList& ret, int key, bool check, wxVariant value) {
			DcmRow row;
			row.push_back(wxString::Format(wxT("%i"),key));
			row.push_back(check);
			row.push_back(value.GetString());
			ret.push_back(row);
		}
		
		////////////////////////////////////////////////////////////////////////
		static void addKeyValueRow(DcmItemList& ret, const char* key, wxVariant value) {
			DcmRow row;
			row.push_back(key);
			row.push_back(value.GetString());
			ret.push_back(row);
		}
		
		////////////////////////////////////////////////////////////////////////
		static void addNumKeyValueRow(DcmItemList& ret, const char* key, wxVariant value) {
			DcmRow row;
			row.push_back(wxString::Format(wxT("%i"),(int)(ret.size() + 1)));
			row.push_back(key);
			row.push_back(value.GetString());
			ret.push_back(row);
		}
		////////////////////////////////////////////////////////////////////////
		static void addNumParameterValueUnitRow(DcmItemList& ret, const char* parameter, wxVariant value, const char* unit ) {
			DcmRow row;
			row.push_back(parameter);
			row.push_back(value.GetString());
			row.push_back(unit);
			ret.push_back(row);
		}
		////////////////////////////////////////////////////////////////////////
		static void addNumKeyValueRow(DcmItemList& ret, unsigned int count, const char* key, wxVariant value) {
			DcmRow row;
			row.push_back(wxString::Format(wxT("%i"),(int)count));
			row.push_back(key);
			row.push_back(value.GetString());
			ret.push_back(row);
		}
		////////////////////////////////////////////////////////////////////////
		static void addNumKeyValueRow(DcmItemList& ret, unsigned int count, unsigned int code, const char* key, wxVariant value) {
			DcmRow row;
			row.push_back(wxString::Format(wxT("%i"),(int)count));
			row.push_back(wxString::Format(wxT("%i"),(int)code));
			row.push_back(key);
			row.push_back(value.GetString());
			ret.push_back(row);
		}
		////////////////////////////////////////////////////////////////////////
		static void addPinReportRow(DcmItemList& ret, const char* desc, int pin, int type, int mode, int value) {
			DcmRow row;
			
			if ( pin >= 0 && pin < MAX_PINS ) {
				row.push_back(wxString::Format(wxT("%i"),(int)pin));
				row.push_back(desc);
				row.push_back((type == (int)'D' ? "Digital" : "Analog"));
				row.push_back((mode == (int)'O' ? "Output" : "Input"));
				row.push_back(wxString::Format(wxT("%i"),(char)value));
				ret.push_back(row);
			} else {
				row.push_back("-");
				row.push_back("Not available, because there's no controller connection");
				row.push_back("-");
				row.push_back("-");
				row.push_back("-");
				ret.push_back(row);
			}
		}
};

#endif