#pragma once
#include "precedence.h"
#include "scanner.h"
#include <stdlib.h>
static void grouping(bool canAssign);
static void unary(bool canAssign);
static void binary(bool canAssign);
static void number(bool canAssign);
static void string(bool canAssign);  // introduced in ch 19 page 347 - in compiler.c
static void variable(bool canAssign);  // introduced in ch 21 page 391
static void literal(bool canAssign);  // new in ch 18 - in compiler.c - used to handle false, true, nil tokens
static void and_(bool canAssign);  // added in Ch 23.2 pg 420
static void or_(bool canAssign);  // added in Ch 23.2.1 pg 421
static void call(bool canAssign);  // added in Ch 24.5 pg 450

typedef void (*ParseFn)(bool canAssign); // ch 21.4 pg 396
//typedef void (*ParseFn)(); // before ch 21

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);



ParseRule rules[];

static ParseRule* getRule(TokenType type) {
	return &rules[type];
}


ParseRule rules[] = {
	// [TOKEN_LEFT_PAREN] = {grouping, NULL,   PREC_NONE},  // up to Ch 24
	[TOKEN_LEFT_PAREN] = {grouping, call,   PREC_CALL}, // Ch 24 function call
	[TOKEN_RIGHT_PAREN] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_LEFT_BRACE] = {NULL,     NULL,   PREC_NONE}, // [big]
	[TOKEN_RIGHT_BRACE] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_COMMA] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_DOT] = {NULL,     NULL,   PREC_NONE},
	// [TOKEN_DOT] = {NULL,     dot,    PREC_CALL},

	[TOKEN_MINUS] = {unary, binary, PREC_TERM},
	[TOKEN_PLUS] = {NULL,     binary, PREC_TERM},
	[TOKEN_SEMICOLON] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_SLASH] = {NULL,     binary, PREC_FACTOR},
	[TOKEN_STAR] = {NULL,     binary, PREC_FACTOR},
	[TOKEN_RANDOM] = {NULL,     binary, PREC_FACTOR},
	//[TOKEN_BANG] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_BANG] = {unary,    NULL,   PREC_NONE}, // ch 18.4.1 Pg 336
	//[TOKEN_BANG_EQUAL] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_BANG_EQUAL] = {NULL,     binary, PREC_EQUALITY}, // ch 18.4.2 Pg 337
	[TOKEN_EQUAL] = {NULL,     NULL,   PREC_NONE},
	
	/* ch 17
	[TOKEN_EQUAL_EQUAL] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_GREATER] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_LESS] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_LESS_EQUAL] = {NULL,     NULL,   PREC_NONE},
	*/

	/*  ch 18.4.2 Pg 337 */
	[TOKEN_EQUAL_EQUAL] =	{NULL,     binary, PREC_EQUALITY},
	[TOKEN_GREATER] =		{NULL,     binary, PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
	[TOKEN_LESS] =			{NULL,     binary, PREC_COMPARISON},
	[TOKEN_LESS_EQUAL] =	{NULL,     binary, PREC_COMPARISON},
	

	//[TOKEN_IDENTIFIER] = {NULL,     NULL,   PREC_NONE},  // this is chapter 17
	[TOKEN_IDENTIFIER] = {variable, NULL,   PREC_NONE}, // this is on page 391 in chapter 21
	
	// [TOKEN_STRING] = {NULL,     NULL,   PREC_NONE}, // ch 18
	[TOKEN_STRING] = {string,   NULL,   PREC_NONE},  // ch 19.3
	
	[TOKEN_NUMBER] = {number,   NULL,   PREC_NONE},
	//[TOKEN_AND] = {NULL,     NULL,   PREC_NONE}, // prior to Ch 23
	[TOKEN_AND] = {NULL,     and_,   PREC_AND},  // Ch 23.2
	// [TOKEN_OR] = {NULL,     NULL,   PREC_NONE}, // prior to Ch 23
	[TOKEN_OR] = {NULL,     or_,    PREC_OR}, // Ch 23.2.1
	[TOKEN_CLASS] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ELSE] = {NULL,     NULL,   PREC_NONE},
	//[TOKEN_FALSE] = {NULL,     NULL,   PREC_NONE}, // this was for ch 17
	[TOKEN_FALSE] = {literal,  NULL,   PREC_NONE},  // new for Ch 18 page 334

	[TOKEN_FOR] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FUN] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_IF] = {NULL,     NULL,   PREC_NONE},
	// [TOKEN_NIL] = {NULL,     NULL,   PREC_NONE},  // this was for ch 17
	[TOKEN_NIL] = {literal,  NULL,   PREC_NONE},
	
	[TOKEN_PRINT] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_RETURN] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_SUPER] = {NULL,     NULL,   PREC_NONE},
	//[TOKEN_SUPER] = {super_,   NULL,   PREC_NONE},
	[TOKEN_THIS] = {NULL,     NULL,   PREC_NONE},
	//[TOKEN_THIS] = {this_,    NULL,   PREC_NONE},
	// [TOKEN_TRUE] = {NULL,     NULL,   PREC_NONE}, // this was for ch 17
	[TOKEN_TRUE] = {literal,  NULL,   PREC_NONE},
	[TOKEN_VAR] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_WHILE] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ERROR] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_EOF] = {NULL,     NULL,   PREC_NONE}
};

